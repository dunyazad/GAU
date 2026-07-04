# Checklist — User-Defined Types

## Phase 1 — Enum (end to end)

### Model
- [ ] Add `UserTypeKind` enum and `UserType` struct (model/UserType.h).
- [ ] Add `StructField` (used later, define now for the struct field).
- [ ] Add a global `UserTypeRegistry` (register / find / list / clear).
- [ ] Add `PinType::UserType` case to GraphTypes.h.
- [ ] Add `typeName` to `Pin`, `PinDef`, `Property` (+ key/element).
- [ ] Update PinTypeToString / PinTypeFromString for the UserType case.
- [ ] `MakeDefaultValue` / `ParseValueString` handle enum (int index).

### Serialization
- [ ] custom_nodes.json gains a `types` array; NodeClassLoader loads it.
- [ ] Save/load `typeName` on pins and properties.
- [ ] Round-trip test: define an enum, save, reload, verify.

### ClassEditorDialog (interaction)
- [ ] UI to create/rename/delete an enum type and its enumerators.
- [ ] PinType dropdown lists builtin types + user types.
- [ ] PropertyType / PropertyKeyType dropdowns list value + user types.
- [ ] Selecting a user type stores UserType + typeName.

### Render
- [ ] Hash-derived pin color for user-type pins.
- [ ] Dropdown rows render user-type names.
- [ ] Property row for an enum edits via a value dropdown (enumerator list).

### Verify
- [ ] Build gau.
- [ ] Manual: define enum, use on a pin + a property, save/reload, undo.

## Phase 2 — Struct (later)
- [ ] Struct fields UI, nested type refs, serialization, rendering.

## Phase 3 — Object alias (later)
- [ ] Opaque type, no members/value; falls out of Phase 1 plumbing.
