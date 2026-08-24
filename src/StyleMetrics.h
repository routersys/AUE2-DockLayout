#pragma once

namespace dl {

struct StyleMetrics {
    int windowSeparator;
    int windowBorder;
    int titleHeader;
    int grouping;
    int windowSeparatorSize;
    int titleHeaderHeight;
    int settingItemHeight;
    int footerHeight;
};

const StyleMetrics& Style();
void RefreshStyle();

}
