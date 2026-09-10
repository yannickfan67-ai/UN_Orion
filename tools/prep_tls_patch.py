#!/usr/bin/env python3
from pathlib import Path
p=Path('kernel/vela.c')
s=p.read_text()
old='p.capabilities=VELA_PLATFORM_CAP_HTTP|VELA_PLATFORM_CAP_LOG|VELA_PLATFORM_CAP_RESOURCES;p.http_get=ohttp;p.resource_get=ores;p.log=olog;(void)vela_init_ex(w,&p,0);'
new='p.capabilities=VELA_PLATFORM_CAP_HTTP|VELA_PLATFORM_CAP_LOG|VELA_PLATFORM_CAP_RESOURCES;p.http_get=ohttp;p.log=olog;p.resource_get=ores;(void)vela_init_ex(w,&p,0);'
if old in s:s=s.replace(old,new,1)
if 'int vela_navigate_action(' not in s:
    anchor='int vela_reload(void){return g_url[0]?nav(g_url,0):0;}'
    nav='int vela_navigate_action(VelaNavAction action){int before=g_scroll;int page=g_viewport_h>96?g_viewport_h-48:48;switch(action){case VELA_NAV_BACK:return vela_back();case VELA_NAV_FORWARD:return vela_forward();case VELA_NAV_RELOAD:return vela_reload();case VELA_NAV_LINE_UP:vela_scroll_by(-48);return g_scroll!=before;case VELA_NAV_LINE_DOWN:vela_scroll_by(48);return g_scroll!=before;case VELA_NAV_PAGE_UP:vela_scroll_by(-page);return g_scroll!=before;case VELA_NAV_PAGE_DOWN:vela_scroll_by(page);return g_scroll!=before;case VELA_NAV_HOME:vela_set_scroll(0);return g_scroll!=before;case VELA_NAV_END:vela_set_scroll(aster_document_height(&g_doc));return g_scroll!=before;default:return 0;}}'
    if anchor not in s:raise SystemExit('missing embedded Vela reload marker')
    s=s.replace(anchor,anchor+'\n'+nav,1)
p.write_text(s)
