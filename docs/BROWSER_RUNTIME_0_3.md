# Browser Runtime 0.3 Compatibility Note

UN_Orion now embeds UN_Vela 0.3 and Aster 0.3 behavior for HTML, the lightweight CSS subset, safe JavaScript subset, link activation, history, scrolling and BMP/PPM image resources.

The desktop exposes Back, Forward and Reload controls and negotiates IntelliMouse wheel packets while retaining fallback compatibility with ordinary 3-byte PS/2 mice.

The Orion-native network carrier currently advertises HTTP and binary-resource capabilities only. HTTPS is not silently downgraded: it remains disabled until a trusted freestanding TLS provider is integrated. Hosted UN_Vela builds provide verified HTTPS on Windows, Linux and macOS through their native/system TLS stacks.
