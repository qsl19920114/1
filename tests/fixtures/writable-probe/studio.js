// T007 Studio Companion facet for @example/chat-scene.
//
// The upstream chat-scene package ships no Studio Companion, so its snapshot
// exposes an empty inspector array and no field accepts parameter.adjust. This
// facet adds the missing declaration layer only; the Surface decoder, manifest
// and renderer are untouched and still come from dist/.
//
// entrance-frames is chosen because activation.ts already declares it as
// kind: "literal" and reads it via element.attributes, and the SVML instance
// carries it as a literal value rather than a {reference}. Both conditions are
// required: parameters.ts:417 computes
//   writable = declaration.writable === true && !isReference
// and parameters.ts:495 emits `edit` only when that resolved writable is true.
//
// control must be stated explicitly: with no public schema on the binding,
// parameterControlForSchema returns undefined and inspectorFieldsForBindings
// throws rather than silently dropping the field (parameters.ts:482).

import { compositionTypes } from "@hypit/hypit/composition";
import { createStudioTrackCompanionHostFacet } from "@hypit/hypit/studio-adapter";

const module = { name: "@example/chat-scene", version: "1" };

export const chatSceneCompanion = {
  id: "scene",
  role: "track",
  output: {
    type: compositionTypes.visualTrack,
    surface: "scene",
    modules: [module],
  },
  family: "media",
  label: "Conversation",
  icon: "component",
  lane: { heightPx: 76 },
  bindings: [
    { name: "entrance-frames", writable: true, fallback: "10" },
    { name: "title", writable: true },
  ],
  inspector: [
    {
      binding: "title",
      label: "标题",
      domain: "how",
      section: { id: "content", label: "内容" },
      control: "text",
    },
    {
      binding: "entrance-frames",
      label: "入场帧数",
      domain: "when",
      section: { id: "timing", label: "时机" },
      control: "number",
      number: { step: 1 },
    },
  ],
};

export const chatSceneStudioFacet = createStudioTrackCompanionHostFacet([chatSceneCompanion]);
