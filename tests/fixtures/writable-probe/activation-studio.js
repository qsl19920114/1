import upstream from "../dist/activation.js";
import { chatSceneStudioFacet } from "./studio.js";

export const hypitPackage = {
  ...upstream,
  hostFacets: [...upstream.hostFacets, chatSceneStudioFacet],
};
export default hypitPackage;
