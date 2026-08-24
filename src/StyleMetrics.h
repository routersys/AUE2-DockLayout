#pragma once

namespace dl {

struct StyleMetrics {
    int windowSeparator;
    int windowBorder;
    int titleHeader;
    int grouping;
    int groupingHover;
    int groupingSelect;
    int windowSeparatorSize;
    int titleHeaderHeight;
    int settingItemHeight;
    int groupTabHeight;
    int footerHeight;
};

const StyleMetrics& Style();
void RefreshStyle();

}
