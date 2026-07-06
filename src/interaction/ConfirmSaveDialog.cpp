#include "ConfirmSaveDialog.h"

void ConfirmSaveDialog::Open(const std::string& name, float screenWidth, float screenHeight)
{
    OpenPrompt("Unsaved Changes", "Save changes to \"" + name + "\" before closing?", "Save",
               "Don't Save", screenWidth, screenHeight);
    documentName = name;
}

void ConfirmSaveDialog::OpenPrompt(const std::string& promptTitle,
                                   const std::string& promptMessage,
                                   const std::string& promptSaveLabel,
                                   const std::string& promptDiscardLabel, float screenWidth,
                                   float screenHeight)
{
    open = true;
    documentName.clear();
    title = promptTitle;
    message = promptMessage;
    saveLabel = promptSaveLabel;
    discardLabel = promptDiscardLabel;
    hoveredButton = -1;
    panelX = (screenWidth - WIDTH) * 0.5f;
    panelY = (screenHeight - HEIGHT) * 0.5f;
    if (panelX < 0.0f) {
        panelX = 0.0f;
    }
    if (panelY < 0.0f) {
        panelY = 0.0f;
    }
}

void ConfirmSaveDialog::Close()
{
    open = false;
    documentName.clear();
    hoveredButton = -1;
}

UIRect ConfirmSaveDialog::PanelRect() const
{
    return UIRect{panelX, panelY, WIDTH, HEIGHT};
}

UIRect ConfirmSaveDialog::SaveButtonRect() const
{
    return UIRect{panelX + WIDTH - PADDING - BUTTON_WIDTH,
                  panelY + HEIGHT - PADDING - BUTTON_HEIGHT, BUTTON_WIDTH, BUTTON_HEIGHT};
}

UIRect ConfirmSaveDialog::DiscardButtonRect() const
{
    return UIRect{panelX + WIDTH - PADDING - BUTTON_WIDTH * 2.0f - BUTTON_GAP,
                  panelY + HEIGHT - PADDING - BUTTON_HEIGHT, BUTTON_WIDTH, BUTTON_HEIGHT};
}

UIRect ConfirmSaveDialog::CancelButtonRect() const
{
    return UIRect{panelX + PADDING, panelY + HEIGHT - PADDING - BUTTON_HEIGHT, BUTTON_WIDTH,
                  BUTTON_HEIGHT};
}

int ConfirmSaveDialog::ButtonAt(float x, float y) const
{
    const UIRect rects[3] = {SaveButtonRect(), DiscardButtonRect(), CancelButtonRect()};
    for (int i = 0; i < 3; ++i) {
        if (x >= rects[i].x && x <= rects[i].x + rects[i].w && y >= rects[i].y
            && y <= rects[i].y + rects[i].h) {
            return i;
        }
    }
    return -1;
}

ConfirmSaveAction ConfirmSaveDialog::HandleEvent(const EditorInputEvent& event)
{
    if (!open) {
        return ConfirmSaveAction::None;
    }

    switch (event.type) {
    case EditorInputType::MouseMove:
        hoveredButton = ButtonAt(event.x, event.y);
        return ConfirmSaveAction::None;

    case EditorInputType::MouseDown: {
        if (event.button != EditorMouseButton::Left) {
            return ConfirmSaveAction::None;
        }
        switch (ButtonAt(event.x, event.y)) {
        case 0:
            return ConfirmSaveAction::Save;
        case 1:
            return ConfirmSaveAction::Discard;
        case 2:
            return ConfirmSaveAction::Cancel;
        default:
            return ConfirmSaveAction::None;
        }
    }

    case EditorInputType::KeyDown:
        if (event.key == EditorKey::Enter) {
            return ConfirmSaveAction::Save;
        }
        if (event.key == EditorKey::Escape) {
            return ConfirmSaveAction::Cancel;
        }
        return ConfirmSaveAction::None;

    default:
        return ConfirmSaveAction::None;
    }
}
