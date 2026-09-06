#include <efi.h>
#include <efilib.h>
#include <elf.h>
#include "bootinfo.h"

static BOOLEAN valid_kernel_elf(const Elf64_Ehdr *eh, UINTN file_size) {
    if (!eh || file_size < sizeof(*eh)) return FALSE;
    if (eh->e_ident[EI_MAG0] != ELFMAG0 ||
        eh->e_ident[EI_MAG1] != ELFMAG1 ||
        eh->e_ident[EI_MAG2] != ELFMAG2 ||
        eh->e_ident[EI_MAG3] != ELFMAG3 ||
        eh->e_ident[EI_CLASS] != ELFCLASS64 ||
        eh->e_ident[EI_DATA] != ELFDATA2LSB ||
        eh->e_machine != EM_X86_64 ||
        eh->e_phentsize != sizeof(Elf64_Phdr)) {
        return FALSE;
    }

    if (eh->e_phoff > file_size) return FALSE;
    UINTN ph_bytes = (UINTN)eh->e_phnum * sizeof(Elf64_Phdr);
    if (ph_bytes > file_size - (UINTN)eh->e_phoff) return FALSE;
    return TRUE;
}

static EFI_STATUS load_kernel(EFI_HANDLE image, EFI_SYSTEM_TABLE *st,
                              EFI_PHYSICAL_ADDRESS *entry_out) {
    EFI_LOADED_IMAGE *loaded = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_FILE_HANDLE root = NULL, file = NULL;
    EFI_FILE_INFO *info = NULL;
    UINTN info_size = 0;
    EFI_STATUS status;

    status = uefi_call_wrapper(st->BootServices->HandleProtocol, 3,
                               image, &LoadedImageProtocol, (void **)&loaded);
    if (EFI_ERROR(status)) return status;

    status = uefi_call_wrapper(st->BootServices->HandleProtocol, 3,
                               loaded->DeviceHandle, &FileSystemProtocol, (void **)&fs);
    if (EFI_ERROR(status)) return status;

    status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
    if (EFI_ERROR(status)) return status;

    status = uefi_call_wrapper(root->Open, 5, root, &file, L"\\kernel.elf",
                               EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) return status;

    status = uefi_call_wrapper(file->GetInfo, 4, file, &GenericFileInfo,
                               &info_size, NULL);
    if (status != EFI_BUFFER_TOO_SMALL) return status;

    status = uefi_call_wrapper(st->BootServices->AllocatePool, 3,
                               EfiLoaderData, info_size, (void **)&info);
    if (EFI_ERROR(status)) return status;

    status = uefi_call_wrapper(file->GetInfo, 4, file, &GenericFileInfo,
                               &info_size, info);
    if (EFI_ERROR(status)) return status;

    UINTN file_size = (UINTN)info->FileSize;
    void *buffer = NULL;
    status = uefi_call_wrapper(st->BootServices->AllocatePool, 3,
                               EfiLoaderData, file_size, &buffer);
    if (EFI_ERROR(status)) return status;

    UINTN bytes_read = file_size;
    status = uefi_call_wrapper(file->Read, 3, file, &bytes_read, buffer);
    if (EFI_ERROR(status) || bytes_read != file_size) return EFI_LOAD_ERROR;

    Elf64_Ehdr *eh = (Elf64_Ehdr *)buffer;
    if (!valid_kernel_elf(eh, file_size)) return EFI_LOAD_ERROR;

    Elf64_Phdr *ph = (Elf64_Phdr *)((UINT8 *)buffer + eh->e_phoff);
    for (UINT16 i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0) continue;
        if (ph[i].p_filesz > ph[i].p_memsz) return EFI_LOAD_ERROR;
        if (ph[i].p_offset > file_size ||
            ph[i].p_filesz > file_size - (UINTN)ph[i].p_offset) {
            return EFI_LOAD_ERROR;
        }

        EFI_PHYSICAL_ADDRESS dest = ph[i].p_paddr;
        UINTN pages = EFI_SIZE_TO_PAGES(ph[i].p_memsz);
        status = uefi_call_wrapper(st->BootServices->AllocatePages, 4,
                                   AllocateAddress, EfiLoaderData, pages, &dest);
        if (EFI_ERROR(status)) return status;

        CopyMem((void *)(UINTN)ph[i].p_paddr,
                (UINT8 *)buffer + ph[i].p_offset,
                ph[i].p_filesz);
        if (ph[i].p_memsz > ph[i].p_filesz) {
            SetMem((UINT8 *)(UINTN)ph[i].p_paddr + ph[i].p_filesz,
                   ph[i].p_memsz - ph[i].p_filesz, 0);
        }
    }

    *entry_out = eh->e_entry;
    return EFI_SUCCESS;
}

static EFI_STATUS exit_boot_services(EFI_HANDLE image, EFI_SYSTEM_TABLE *st,
                                     OrionBootInfo *bi) {
    EFI_MEMORY_DESCRIPTOR *map = NULL;
    UINTN map_capacity = 0;
    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_version = 0;
    EFI_STATUS status;

    status = uefi_call_wrapper(st->BootServices->GetMemoryMap, 5,
                               &map_size, NULL, &map_key,
                               &desc_size, &desc_version);
    if (status != EFI_BUFFER_TOO_SMALL || desc_size == 0) return status;

    map_capacity = map_size + desc_size * 8;
    status = uefi_call_wrapper(st->BootServices->AllocatePool, 3,
                               EfiLoaderData, map_capacity, (void **)&map);
    if (EFI_ERROR(status)) return status;

    for (;;) {
        map_size = map_capacity;
        status = uefi_call_wrapper(st->BootServices->GetMemoryMap, 5,
                                   &map_size, map, &map_key,
                                   &desc_size, &desc_version);
        if (status == EFI_BUFFER_TOO_SMALL) {
            EFI_MEMORY_DESCRIPTOR *larger = NULL;
            map_capacity = map_size + desc_size * 8;
            status = uefi_call_wrapper(st->BootServices->AllocatePool, 3,
                                       EfiLoaderData, map_capacity,
                                       (void **)&larger);
            if (EFI_ERROR(status)) return status;
            uefi_call_wrapper(st->BootServices->FreePool, 1, map);
            map = larger;
            continue;
        }
        if (EFI_ERROR(status)) return status;

        bi->memory_map = (uint64_t)(uintptr_t)map;
        bi->memory_map_size = map_size;
        bi->memory_descriptor_size = desc_size;
        bi->memory_descriptor_version = desc_version;

        status = uefi_call_wrapper(st->BootServices->ExitBootServices, 2,
                                   image, map_key);
        if (status == EFI_SUCCESS) return EFI_SUCCESS;
        if (status != EFI_INVALID_PARAMETER) return status;
        /* The map changed between GetMemoryMap and ExitBootServices. Retry. */
    }
}

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    InitializeLib(image, st);
    Print(L"UN_Orion bootloader\r\n");

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_STATUS status = uefi_call_wrapper(st->BootServices->LocateProtocol, 3,
                                          &GraphicsOutputProtocol, NULL,
                                          (void **)&gop);
    if (EFI_ERROR(status)) {
        Print(L"GOP not available\r\n");
        return status;
    }
    if (!gop->Mode || !gop->Mode->Info ||
        gop->Mode->Info->PixelFormat == PixelBltOnly ||
        gop->Mode->FrameBufferBase == 0) {
        Print(L"GOP framebuffer mode unsupported\r\n");
        return EFI_UNSUPPORTED;
    }

    EFI_PHYSICAL_ADDRESS kernel_entry = 0;
    status = load_kernel(image, st, &kernel_entry);
    if (EFI_ERROR(status)) {
        Print(L"kernel.elf load failed: %r\r\n", status);
        return status;
    }

    OrionBootInfo *bi = NULL;
    status = uefi_call_wrapper(st->BootServices->AllocatePool, 3,
                               EfiLoaderData, sizeof(*bi), (void **)&bi);
    if (EFI_ERROR(status)) return status;

    bi->framebuffer_base = gop->Mode->FrameBufferBase;
    bi->framebuffer_size = gop->Mode->FrameBufferSize;
    bi->width = gop->Mode->Info->HorizontalResolution;
    bi->height = gop->Mode->Info->VerticalResolution;
    bi->pixels_per_scanline = gop->Mode->Info->PixelsPerScanLine;
    bi->pixel_format = gop->Mode->Info->PixelFormat;

    status = exit_boot_services(image, st, bi);
    if (EFI_ERROR(status)) return status;

    void (*kernel_main)(OrionBootInfo *) =
        (void (*)(OrionBootInfo *))(UINTN)kernel_entry;
    kernel_main(bi);

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}
