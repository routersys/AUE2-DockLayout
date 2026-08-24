#include "StyleMetrics.h"

#include "HostContext.h"

namespace dl {

static StyleMetrics g_style = {};
static bool g_loaded = false;

void RefreshStyle() {
    g_style.windowSeparator     = ColorCode("WindowSeparator");
    g_style.windowBorder        = ColorCode("WindowBorder");
    g_style.titleHeader         = ColorCode("TitleHeader");
    g_style.grouping            = ColorCode("Grouping");
    g_style.groupingHover       = ColorCode("GroupingHover");
    g_style.groupingSelect      = ColorCode("GroupingSelect");
    g_style.windowSeparatorSize = LayoutSize("WindowSeparatorSize");
    g_style.titleHeaderHeight   = LayoutSize("TitleHeaderHeight");
    g_style.settingItemHeight   = LayoutSize("SettingItemHeight");
    g_style.groupTabHeight      = LayoutSize("GroupTabHeight");
    g_style.footerHeight        = LayoutSize("FooterHeight");
    g_loaded = true;
}

const StyleMetrics& Style() {
    if (!g_loaded) RefreshStyle();
    return g_style;
}

}
