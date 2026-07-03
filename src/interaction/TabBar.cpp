#include "TabBar.h"

UIRect TabRect(int tabIndex)
{
    return UIRect{TAB_WIDTH * static_cast<float>(tabIndex), 0.0f, TAB_WIDTH, TAB_BAR_HEIGHT};
}

UIRect TabCloseRect(int tabIndex)
{
    const UIRect tab = TabRect(tabIndex);
    return UIRect{tab.x + tab.w - TAB_CLOSE_WIDTH, tab.y, TAB_CLOSE_WIDTH, tab.h};
}

UIRect NewTabRect(int tabCount)
{
    return UIRect{TAB_WIDTH * static_cast<float>(tabCount), 0.0f, TAB_NEW_WIDTH, TAB_BAR_HEIGHT};
}

UIRect SaveAsButtonRect(float screenWidth)
{
    return UIRect{screenWidth - TAB_SAVE_AS_WIDTH - TAB_BUTTON_MARGIN,
                  TAB_BUTTON_MARGIN,
                  TAB_SAVE_AS_WIDTH,
                  TAB_BAR_HEIGHT - TAB_BUTTON_MARGIN * 2.0f};
}

UIRect SaveButtonRect(float screenWidth)
{
    const UIRect saveAs = SaveAsButtonRect(screenWidth);
    return UIRect{saveAs.x - TAB_OPEN_SAVE_WIDTH - TAB_BUTTON_MARGIN,
                  saveAs.y, TAB_OPEN_SAVE_WIDTH, saveAs.h};
}

UIRect OpenButtonRect(float screenWidth)
{
    const UIRect save = SaveButtonRect(screenWidth);
    return UIRect{save.x - TAB_OPEN_SAVE_WIDTH - TAB_BUTTON_MARGIN,
                  save.y, TAB_OPEN_SAVE_WIDTH, save.h};
}

UIRect RunButtonRect(float screenWidth)
{
    const UIRect open = OpenButtonRect(screenWidth);
    return UIRect{open.x - TAB_OPEN_SAVE_WIDTH - TAB_BUTTON_MARGIN,
                  open.y, TAB_OPEN_SAVE_WIDTH, open.h};
}

TabBarHit HitTestTabBar(float x, float y, int tabCount, float screenWidth)
{
    TabBarHit hit;
    if (y < 0.0f || y > TAB_BAR_HEIGHT) {
        return hit;
    }

    if (RunButtonRect(screenWidth).Contains(x, y)) {
        hit.kind = TabBarHit::Kind::Run;
        return hit;
    }
    if (OpenButtonRect(screenWidth).Contains(x, y)) {
        hit.kind = TabBarHit::Kind::Open;
        return hit;
    }
    if (SaveButtonRect(screenWidth).Contains(x, y)) {
        hit.kind = TabBarHit::Kind::Save;
        return hit;
    }
    if (SaveAsButtonRect(screenWidth).Contains(x, y)) {
        hit.kind = TabBarHit::Kind::SaveAs;
        return hit;
    }
    if (NewTabRect(tabCount).Contains(x, y)) {
        hit.kind = TabBarHit::Kind::NewTab;
        return hit;
    }
    for (int i = 0; i < tabCount; ++i) {
        if (TabCloseRect(i).Contains(x, y)) {
            hit.kind = TabBarHit::Kind::CloseTab;
            hit.tabIndex = i;
            return hit;
        }
        if (TabRect(i).Contains(x, y)) {
            hit.kind = TabBarHit::Kind::Tab;
            hit.tabIndex = i;
            return hit;
        }
    }
    return hit;
}
