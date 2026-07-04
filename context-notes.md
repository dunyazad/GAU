# Context Notes — User-Defined Types

Ongoing decisions for the user-defined type system. Append as work proceeds.

## Goal

Let users define their own data types and select them wherever a PinType is
chosen (pin type dropdown, property type, property key type). Kinds: Enum,
Struct, Object alias. Managed inside the existing ClassEditorDialog. Applies
to both pin types and node properties.

## Key design decisions

### Type reference model
- `PinType` gains a new case `UserType`.
- `Pin`, `PinDef`, `Property` (and key/element type fields) gain a
  `std::string typeName`, meaningful only when the type is `UserType`. It
  names which user type is referenced.
- Rationale: mirrors how categories are free-form strings; keeps the fixed
  builtin enum intact while allowing an open set of user types.

### Type registry
- New model type `UserType { std::string name; UserTypeKind kind;
  std::vector<std::string> enumerators; std::vector<StructField> fields; }`.
- `UserTypeKind { Enum, Struct, ObjectAlias }`.
- `StructField { std::string name; PinType type; std::string typeName; }`.
- Global registry like `NodeClass::GetRegistry()` — types are shared across
  graphs, so they live in the global custom definitions file, not per-graph.

### Value model (exec)
- Enum -> `int` (enumerator index). Fits the existing Value variant.
- ObjectAlias -> no runtime value (like Object today).
- Struct -> no runtime value initially (deferred); treated like Object for
  exec. Full struct values would need a Value redesign — out of scope now.

### Colors
- User-type pins get a hash-derived color (reuse the category hash-color
  helper) so they are visually distinct without palette entries.

### Serialization
- Global custom definitions file (custom_nodes.json, via NodeClassLoader)
  gains a top-level `types` array. Each entry: name, kind, enumerators or
  fields. Pins/properties serialize `typeName` alongside the type string
  ("usertype" or the type name itself — decide during Phase 1).

### ClassEditorDialog UI
- Extend the existing dialog with a way to manage types (not just classes).
  Exact layout TBD in Phase 1 (candidate: an editor-target toggle
  "Class | Type", reusing the row/dropdown widgets already there).

## Phasing (do NOT do all at once)

- Phase 1 — Enum, end to end. Proves the type-reference plumbing
  (PinType::UserType + typeName + registry + dropdowns + serialize + color)
  with the simplest value model (enum = int).
- Phase 2 — Struct (fields, nested type refs; hardest; value model deferred).
- Phase 3 — Object alias (trivial once Phase 1 plumbing exists: opaque,
  no members, no value).

## Status

- Phase 1 backend DONE and green (build + all tests pass):
  - Model foundation: PinType::UserType, UserType/UserTypeRegistry,
    typeName on Pin/PinDef/PropertyDef (+keyTypeName), value = enum int.
  - Serialization: pins/properties reference user types by name; a top
    level `types` array in custom_nodes.json loads before classes and is
    saved via SaveUserTypeToFile / removed via RemoveUserTypeFromFile.
  - Serialized type string carries the user type NAME (round-trips through
    ResolveTypeString); no separate typeName JSON field.
- Phase 1 UI DONE and green (build + all tests pass):
  - Dialog dropdowns (pin/property/key type) list builtin + user types via
    a typeOptions list; selection stores type + typeName.
  - In-dialog type authoring: ClassEditorDialog now has "Node Class | Type"
    mode tabs. Type mode: name + kind dropdown (Enum/Struct/Object) +
    editable enum value rows + "Save Type". Submit -> UserTypeRegistry +
    SaveUserTypeToFile. Entry point is the existing "+ Create New Class..."
    menu, then the Type tab.
  - Renderer: type-cell labels show user type name; user-type pins use a
    single distinct color (purple); per-type hash is deferred polish.
- Object alias (Phase 3) already works via the generic path (authoring
  name+kind, pin selection, serialize). Only STRUCT FIELDS UI (Phase 2)
  and the enum-property VALUE dropdown (polish) remain.
- Phase 2 (Struct) DONE and green: Type tab's member section is now
  kind-aware. kind==Struct shows editable field rows (name + type
  dropdown + remove) with a FieldType dropdown that lists builtins
  (Bool/Int/Float/String/Object, no Exec) + all user types (nesting).
  TrySubmitType builds UserType.fields; serialized via BuildUserTypeEntry.
  So a struct like Vector3f {x,y,z: float} can be authored and then picked
  in the Pins type dropdown.
- Motivation captured: the whole feature started because the Pins data
  type dropdown lacked user types like Vector3f. Now the PIN type dropdown
  and the struct FIELD type dropdown both list all user types.
- Consistency caveat: property value type / key type dropdowns still list
  only value builtins + enums (structs/object aliases have no scalar Value
  representation, so they cannot be a property's value type). If the user
  wants struct-valued properties, that needs a Value-model extension
  (deferred). All OTHER type selectors list user types uniformly.
- Polish DONE and green (build + tests):
  - Per-user-type hash pin color: PinLayout carries typeName; PinStyle has
    UserTypeColor(name) (FNV-1a -> hue) and PinColorForType(type, name);
    node + link renderers pass it. Each user type gets a stable color.
  - Edit existing type: Type tab has a "Load" dropdown listing registry
    types; selecting one prefills the form and sets typeEditOldName. On
    submit, a rename removes the old registry + file entry.
  - Enum property value editing: an enum-typed scalar property cycles to
    the next enumerator on click (like bool toggle) and displays the
    enumerator NAME (PropertyValueToText resolves it via the registry).
- Struct-VALUED properties DONE and green (build + tests + startup smoke).
  Implemented WITHOUT touching the Value variant (lower risk): PropertyValue
  gained a `structFields` vector (recursive). Details:
  - Property type dropdowns now accept structs (BuildTypeOptions filter is
    excludeObjectAlias; structs allowed, object aliases excluded). Struct
    is scalar-only: array/set/map element and map-key structs are rejected
    in both ClassEditor (ConvertPropertyDraft) and the loader.
  - MakeDefaultPropertyValue (PropertyText) builds nested struct defaults;
    NodeGraph::AddNode uses it. GraphSerializer round-trips struct values
    as JSON objects (StructValueToJson / JsonToStructValue).
  - PropertyPanel expands a struct property into a header row + one
    indented editable row per field (BuildRows / PropertyRow). Fields edit
    as text / bool toggle / enum cycle; nested struct fields show "{...}"
    (not inline-editable). SetNodePropertyCommand gained a fieldIndex to
    set one field undoably; ApplyPropertyPanelAction parses per field type.
  - PinStyle links (CMake): NodeGraph now depends on PropertyText, so
    serializer_tests and exec_tests link PropertyText.cpp + UserType.cpp.
- Everything in the user-defined types feature is now complete: enum,
  struct, object alias -- authoring, pin/property use, colors, editing,
  serialization. Not yet committed.

## Related prior work (this session)

- Alignment + auto-layout feature landed: src/interaction/NodeAlign.{h,cpp},
  ActionMenu submenu support, ContextMenu "Arrange All Nodes". Built OK, not
  yet committed.
