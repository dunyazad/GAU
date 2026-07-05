#include "ClassEditorDialogRenderer.h"

#include "interaction/ClassEditorDialog.h"

#include "model/UserType.h"

#include <nanovg.h>

#include <string>

static const char* FONT_REGULAR = "sans";
static const char* FONT_BOLD = "sans-bold";
static const float DIALOG_FONT_SIZE = 13.0f * UI_SCALE;
static const float DIALOG_TITLE_FONT_SIZE = 15.0f * UI_SCALE;

static const char* PinTypeDisplayName(PinType type)
{
    switch (type) {
    case PinType::Exec:
        return "Exec";
    case PinType::Bool:
        return "Bool";
    case PinType::Int:
        return "Int";
    case PinType::Float:
        return "Float";
    case PinType::String:
        return "String";
    case PinType::Object:
        return "Object";
    case PinType::UserType:
        return "";
    }
    return "";
}

// Closed-dropdown label for a type cell: the user type name when set,
// else the builtin display name.
static std::string TypeCellLabel(PinType type, const std::string& typeName)
{
    if (type == PinType::UserType) {
        return typeName;
    }
    return PinTypeDisplayName(type);
}

static const char* KindDisplayName(UserTypeKind kind)
{
    switch (kind) {
    case UserTypeKind::Enum:
        return "Enum";
    case UserTypeKind::Struct:
        return "Struct";
    case UserTypeKind::ObjectAlias:
        return "Object";
    }
    return "";
}

static const UserTypeKind KIND_ORDER[3] = {
    UserTypeKind::Enum,
    UserTypeKind::Struct,
    UserTypeKind::ObjectAlias,
};

static const char* ContainerDisplayName(PropertyContainer container)
{
    switch (container) {
    case PropertyContainer::None:
        return "None";
    case PropertyContainer::Array:
        return "Array";
    case PropertyContainer::Set:
        return "Set";
    case PropertyContainer::Map:
        return "Map";
    }
    return "";
}

static const char* DropdownOptionLabel(const ClassEditorDialog& dialog, int optionIndex)
{
    switch (dialog.GetDropdownKind()) {
    case DialogDropdownKind::None:
        return "";
    case DialogDropdownKind::Category:
        return dialog.GetCategoryOptions()[static_cast<std::size_t>(optionIndex)].c_str();
    case DialogDropdownKind::PinType:
    case DialogDropdownKind::PropertyType:
    case DialogDropdownKind::PropertyKeyType:
    case DialogDropdownKind::FieldType:
        return dialog.GetTypeOptions()[static_cast<std::size_t>(optionIndex)].label.c_str();
    case DialogDropdownKind::PropertyContainer:
        return ContainerDisplayName(ALL_PROPERTY_CONTAINERS[optionIndex]);
    case DialogDropdownKind::TypeKind:
        return KindDisplayName(KIND_ORDER[optionIndex]);
    case DialogDropdownKind::LoadType:
        return UserTypeRegistry::GetAll()[static_cast<std::size_t>(optionIndex)].name.c_str();
    }
    return "";
}

static void DrawButton(NVGcontext* vg, const UIRect& rect, const char* label, bool accent)
{
    nvgBeginPath(vg);
    nvgRoundedRect(vg, rect.x, rect.y, rect.w, rect.h, 3.0f * UI_SCALE);
    nvgFillColor(vg, accent ? nvgRGB(50, 90, 160) : nvgRGB(45, 45, 50));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(70, 70, 78));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, DIALOG_FONT_SIZE);
    nvgFillColor(vg, nvgRGB(230, 230, 235));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, label, nullptr);
}

// Dropdown closed state: label left-aligned plus a small down arrow.
static void DrawDropdownButton(NVGcontext* vg, const UIRect& rect, const char* label, bool enabled)
{
    nvgBeginPath(vg);
    nvgRoundedRect(vg, rect.x, rect.y, rect.w, rect.h, 3.0f * UI_SCALE);
    nvgFillColor(vg, enabled ? nvgRGB(45, 45, 50) : nvgRGB(32, 32, 36));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(70, 70, 78));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgSave(vg);
    nvgIntersectScissor(vg, rect.x, rect.y, rect.w - 12.0f * UI_SCALE, rect.h);
    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, DIALOG_FONT_SIZE);
    nvgFillColor(vg, enabled ? nvgRGB(230, 230, 235) : nvgRGB(110, 110, 118));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, rect.x + 6.0f * UI_SCALE, rect.y + rect.h * 0.5f, label, nullptr);
    nvgRestore(vg);

    const float arrowCenterX = rect.x + rect.w - 8.0f * UI_SCALE;
    const float arrowCenterY = rect.y + rect.h * 0.5f;
    nvgBeginPath(vg);
    nvgMoveTo(vg, arrowCenterX - 3.5f * UI_SCALE, arrowCenterY - 2.0f * UI_SCALE);
    nvgLineTo(vg, arrowCenterX + 3.5f * UI_SCALE, arrowCenterY - 2.0f * UI_SCALE);
    nvgLineTo(vg, arrowCenterX, arrowCenterY + 2.5f * UI_SCALE);
    nvgClosePath(vg);
    nvgFillColor(vg, enabled ? nvgRGB(170, 170, 178) : nvgRGB(90, 90, 96));
    nvgFill(vg);
}

static void DrawTextField(NVGcontext* vg, const UIRect& rect, const std::string& text,
                          const char* placeholder, bool focused)
{
    nvgBeginPath(vg);
    nvgRoundedRect(vg, rect.x, rect.y, rect.w, rect.h, 3.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGB(15, 15, 17));
    nvgFill(vg);
    nvgStrokeColor(vg, focused ? nvgRGB(70, 110, 180) : nvgRGB(60, 60, 66));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, DIALOG_FONT_SIZE);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    nvgSave(vg);
    nvgIntersectScissor(vg, rect.x, rect.y, rect.w, rect.h);
    const float textX = rect.x + 7.0f * UI_SCALE;
    const float textY = rect.y + rect.h * 0.5f;
    if (text.empty() && !focused) {
        nvgFillColor(vg, nvgRGB(110, 110, 118));
        nvgText(vg, textX, textY, placeholder, nullptr);
    } else {
        nvgFillColor(vg, nvgRGB(235, 235, 240));
        const std::string shown = focused ? text + "|" : text;
        nvgText(vg, textX, textY, shown.c_str(), nullptr);
    }
    nvgRestore(vg);
}

static void DrawRowLabel(NVGcontext* vg, float x, float centerY, const char* label)
{
    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, DIALOG_FONT_SIZE);
    nvgFillColor(vg, nvgRGB(170, 170, 178));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x, centerY, label, nullptr);
}

static void DrawPinRows(NVGcontext* vg, const ClassEditorDialog& dialog)
{
    const std::vector<PinDef>& pins = dialog.GetPins();
    for (int i = 0; i < static_cast<int>(pins.size()); ++i) {
        const PinDef& pin = pins[static_cast<std::size_t>(i)];

        DrawButton(vg, dialog.PinDirectionRect(i),
                   pin.direction == PinDirection::Input ? "In" : "Out", false);
        const std::string pinTypeLabel = TypeCellLabel(pin.type, pin.typeName);
        DrawDropdownButton(vg, dialog.PinTypeRect(i), pinTypeLabel.c_str(), true);

        const bool focused = dialog.GetFocus() == ClassEditorDialog::Focus::PinName
                          && dialog.GetFocusedPinIndex() == i;
        DrawTextField(vg, dialog.PinNameRect(i), pin.name, "Pin name", focused);

        DrawButton(vg, dialog.PinRemoveRect(i), "x", false);
    }
}

static void DrawPropertyRows(NVGcontext* vg, const ClassEditorDialog& dialog)
{
    const std::vector<PropertyDraft>& properties = dialog.GetProperties();
    for (int i = 0; i < static_cast<int>(properties.size()); ++i) {
        const PropertyDraft& property = properties[static_cast<std::size_t>(i)];

        DrawDropdownButton(vg, dialog.PropertyContainerRect(i),
                           ContainerDisplayName(property.container), true);
        const std::string propTypeLabel = TypeCellLabel(property.type, property.typeName);
        DrawDropdownButton(vg, dialog.PropertyTypeRect(i), propTypeLabel.c_str(), true);

        const bool isMap = property.container == PropertyContainer::Map;
        const std::string keyTypeLabel = TypeCellLabel(property.keyType, property.keyTypeName);
        DrawDropdownButton(vg, dialog.PropertyKeyTypeRect(i),
                           isMap ? keyTypeLabel.c_str() : "-", isMap);

        const bool nameFocused = dialog.GetFocus() == ClassEditorDialog::Focus::PropertyName
                              && dialog.GetFocusedPropertyIndex() == i;
        DrawTextField(vg, dialog.PropertyNameRect(i), property.name, "Name", nameFocused);

        const bool defaultFocused = dialog.GetFocus() == ClassEditorDialog::Focus::PropertyDefault
                                 && dialog.GetFocusedPropertyIndex() == i;
        const char* placeholder = "Default";
        if (property.container == PropertyContainer::Array
            || property.container == PropertyContainer::Set) {
            placeholder = "a, b, c";
        } else if (isMap) {
            placeholder = "k:v, k:v";
        }
        DrawTextField(vg, dialog.PropertyDefaultRect(i), property.defaultText, placeholder,
                      defaultFocused);

        DrawButton(vg, dialog.PropertyRemoveRect(i), "x", false);
    }
}

static void DrawOpenDropdown(NVGcontext* vg, const ClassEditorDialog& dialog)
{
    if (dialog.GetDropdownKind() == DialogDropdownKind::None) {
        return;
    }

    const UIRect list = dialog.DropdownListRect();
    nvgBeginPath(vg);
    nvgRoundedRect(vg, list.x, list.y, list.w, list.h, 3.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGBA(18, 18, 21, 250));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(80, 80, 88));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, DIALOG_FONT_SIZE);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    const int selectedIndex = dialog.GetDropdownSelectedIndex();
    for (int i = 0; i < dialog.GetDropdownOptionCount(); ++i) {
        const UIRect option = dialog.DropdownOptionRect(i);

        if (i == dialog.GetDropdownHoverIndex()) {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, option.x + 2.0f * UI_SCALE, option.y,
                           option.w - 4.0f * UI_SCALE, option.h, 2.0f * UI_SCALE);
            nvgFillColor(vg, nvgRGBA(70, 110, 180, 220));
            nvgFill(vg);
        }

        nvgFillColor(vg, (i == selectedIndex) ? nvgRGB(140, 180, 240) : nvgRGB(225, 225, 230));
        nvgText(vg, option.x + 8.0f * UI_SCALE, option.y + option.h * 0.5f,
                DropdownOptionLabel(dialog, i), nullptr);
    }
}

static void DrawTab(NVGcontext* vg, const UIRect& rect, const char* label, bool active)
{
    nvgBeginPath(vg);
    nvgRoundedRect(vg, rect.x, rect.y, rect.w, rect.h, 3.0f * UI_SCALE);
    nvgFillColor(vg, active ? nvgRGB(50, 90, 160) : nvgRGB(36, 36, 40));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(70, 70, 78));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, DIALOG_FONT_SIZE);
    nvgFillColor(vg, active ? nvgRGB(235, 235, 240) : nvgRGB(160, 160, 168));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, label, nullptr);
}

static void DrawTabs(NVGcontext* vg, const ClassEditorDialog& dialog)
{
    DrawTab(vg, dialog.TabClassRect(), "Node Class",
            dialog.GetMode() == DialogEditMode::Class);
    DrawTab(vg, dialog.TabTypeRect(), "Type", dialog.GetMode() == DialogEditMode::Type);
}

static void DrawTypeModeContent(NVGcontext* vg, const ClassEditorDialog& dialog, float x)
{
    const UIRect nameRect = dialog.TypeNameFieldRect();
    DrawRowLabel(vg, x + ClassEditorDialog::PADDING, nameRect.y + nameRect.h * 0.5f, "Name");
    DrawTextField(vg, nameRect, dialog.GetTypeNameText(), "Type name",
                  dialog.GetFocus() == ClassEditorDialog::Focus::TypeName);
    DrawDropdownButton(vg, dialog.LoadTypeRect(), "Load", true);

    const UIRect kindRect = dialog.TypeKindRect();
    DrawRowLabel(vg, x + ClassEditorDialog::PADDING, kindRect.y + kindRect.h * 0.5f, "Kind");
    DrawDropdownButton(vg, kindRect, KindDisplayName(dialog.GetTypeKind()), true);

    if (dialog.GetTypeKind() == UserTypeKind::Enum) {
        DrawRowLabel(vg, x + ClassEditorDialog::PADDING, dialog.EnumValuesLabelCenterY(), "Values");
        const std::vector<std::string>& values = dialog.GetEnumValues();
        for (int i = 0; i < static_cast<int>(values.size()); ++i) {
            const bool focused = dialog.GetFocus() == ClassEditorDialog::Focus::EnumValue
                              && dialog.GetFocusedEnumIndex() == i;
            DrawTextField(vg, dialog.EnumValueRect(i), values[static_cast<std::size_t>(i)],
                          "Value", focused);
            DrawButton(vg, dialog.EnumValueRemoveRect(i), "x", false);
        }
        DrawButton(vg, dialog.AddEnumValueButtonRect(), "+ Add Value", false);
    } else if (dialog.GetTypeKind() == UserTypeKind::Struct) {
        DrawRowLabel(vg, x + ClassEditorDialog::PADDING, dialog.EnumValuesLabelCenterY(), "Fields");
        const std::vector<StructField>& fields = dialog.GetStructFields();
        for (int i = 0; i < static_cast<int>(fields.size()); ++i) {
            const StructField& field = fields[static_cast<std::size_t>(i)];
            const bool focused = dialog.GetFocus() == ClassEditorDialog::Focus::StructFieldName
                              && dialog.GetFocusedEnumIndex() == i;
            DrawTextField(vg, dialog.FieldNameRect(i), field.name, "Field name", focused);
            const std::string fieldTypeLabel = TypeCellLabel(field.type, field.typeName);
            DrawDropdownButton(vg, dialog.FieldTypeRect(i), fieldTypeLabel.c_str(), true);
            DrawButton(vg, dialog.EnumValueRemoveRect(i), "x", false);
        }
        DrawButton(vg, dialog.AddEnumValueButtonRect(), "+ Add Field", false);
    }
}

void DrawClassEditorDialog(NVGcontext* vg, const ClassEditorDialog& dialog,
                           float screenWidth, float screenHeight)
{
    if (!dialog.IsOpen()) {
        return;
    }

    // Modal dim overlay.
    nvgBeginPath(vg);
    nvgRect(vg, 0.0f, 0.0f, screenWidth, screenHeight);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 120));
    nvgFill(vg);

    const float x = dialog.GetX();
    const float y = dialog.GetY();
    const float width = ClassEditorDialog::WIDTH;
    const float height = dialog.GetPanelHeight();

    // Panel.
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, 5.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGBA(24, 24, 28, 250));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(70, 70, 78));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // Title.
    nvgFontFace(vg, FONT_BOLD);
    nvgFontSize(vg, DIALOG_TITLE_FONT_SIZE);
    nvgFillColor(vg, nvgRGB(240, 240, 245));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + ClassEditorDialog::PADDING,
            y + ClassEditorDialog::PADDING + ClassEditorDialog::TITLE_HEIGHT * 0.5f,
            dialog.IsEditMode() ? "Edit Node Class" : "Create", nullptr);

    DrawTabs(vg, dialog);

    if (dialog.GetMode() == DialogEditMode::Type) {
        DrawTypeModeContent(vg, dialog, x);
    } else {
        // Name row.
        const UIRect nameRect = dialog.NameFieldRect();
        DrawRowLabel(vg, x + ClassEditorDialog::PADDING, nameRect.y + nameRect.h * 0.5f, "Name");
        DrawTextField(vg, nameRect, dialog.GetClassNameText(), "Class name",
                      dialog.GetFocus() == ClassEditorDialog::Focus::ClassName);

        // Category row: editable text field plus a suggestion dropdown arrow.
        const UIRect categoryRect = dialog.CategoryFieldRect();
        DrawRowLabel(vg, x + ClassEditorDialog::PADDING, categoryRect.y + categoryRect.h * 0.5f,
                     "Category");
        DrawTextField(vg, categoryRect, dialog.GetCategoryText(), "Category",
                      dialog.GetFocus() == ClassEditorDialog::Focus::Category);
        DrawDropdownButton(vg, dialog.CategoryDropdownRect(), "", true);

        // Pins section.
        DrawRowLabel(vg, x + ClassEditorDialog::PADDING, dialog.PinsLabelCenterY(), "Pins");
        DrawPinRows(vg, dialog);
        DrawButton(vg, dialog.AddPinButtonRect(), "+ Add Pin", false);

        // Properties section.
        DrawRowLabel(vg, x + ClassEditorDialog::PADDING, dialog.PropertiesLabelCenterY(),
                     "Properties");
        DrawPropertyRows(vg, dialog);
        DrawButton(vg, dialog.AddPropertyButtonRect(), "+ Add Property", false);
    }

    // Error line.
    const std::string& errorText = dialog.GetErrorText();
    if (!errorText.empty()) {
        nvgFontFace(vg, FONT_REGULAR);
        nvgFontSize(vg, DIALOG_FONT_SIZE);
        nvgFillColor(vg, nvgRGB(230, 90, 90));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, x + ClassEditorDialog::PADDING, dialog.ErrorCenterY(),
                errorText.c_str(), nullptr);
    }

    const char* okLabel = (dialog.GetMode() == DialogEditMode::Type)
                              ? "Save Type"
                              : (dialog.IsEditMode() ? "Save" : "Create");
    DrawButton(vg, dialog.OkButtonRect(), okLabel, true);
    DrawButton(vg, dialog.CancelButtonRect(), "Cancel", false);
    if (dialog.CanDelete()) {
        DrawButton(vg, dialog.DeleteButtonRect(), "Delete", false);
    }

    // Open dropdown draws on top of everything else.
    DrawOpenDropdown(vg, dialog);
}
