#pragma once

// In-app modal "save changes before closing?" dialog, rendered with the
// editor's own NanoVG theme (the native OS message box clashed with the
// dark UI). Modal: consumes every event while open. Enter picks Save,
// Escape picks Cancel.

#include "EditorInputEvent.h"
#include "UIRect.h"
#include "UIScale.h"

#include <string>

enum class ConfirmSaveAction
{
    None,
    Save,
    Discard,
    Cancel,
};

class ConfirmSaveDialog
{
public:
    static constexpr float WIDTH = 420.0f * UI_SCALE;
    static constexpr float HEIGHT = 132.0f * UI_SCALE;
    static constexpr float PADDING = 14.0f * UI_SCALE;
    static constexpr float TITLE_HEIGHT = 26.0f * UI_SCALE;
    static constexpr float BUTTON_WIDTH = 112.0f * UI_SCALE;
    static constexpr float BUTTON_HEIGHT = 30.0f * UI_SCALE;
    static constexpr float BUTTON_GAP = 10.0f * UI_SCALE;

    bool IsOpen() const { return open; }

    void Open(const std::string& name, float screenWidth, float screenHeight);
    // Same three-button modal with custom texts (e.g. the autosave
    // recovery prompt). Save/Discard/Cancel semantics stay with the
    // caller; the labels are display-only.
    void OpenPrompt(const std::string& promptTitle, const std::string& promptMessage,
                    const std::string& saveLabel, const std::string& discardLabel,
                    float screenWidth, float screenHeight);
    void Close();

    // Consumes all events while open. Returns the chosen action, or
    // None while the user is still deciding.
    ConfirmSaveAction HandleEvent(const EditorInputEvent& event);

    const std::string& GetDocumentName() const { return documentName; }
    const std::string& GetTitle() const { return title; }
    const std::string& GetMessage() const { return message; }
    const std::string& GetSaveLabel() const { return saveLabel; }
    const std::string& GetDiscardLabel() const { return discardLabel; }
    // 0 = Save, 1 = Don't Save, 2 = Cancel, -1 = none.
    int GetHoveredButton() const { return hoveredButton; }

    UIRect PanelRect() const;
    UIRect SaveButtonRect() const;
    UIRect DiscardButtonRect() const;
    UIRect CancelButtonRect() const;

private:
    int ButtonAt(float x, float y) const;

    bool open = false;
    std::string documentName;
    std::string title;
    std::string message;
    std::string saveLabel;
    std::string discardLabel;
    float panelX = 0.0f;
    float panelY = 0.0f;
    int hoveredButton = -1;
};
