## Questions

* (#area_diffuse) In the context of representative points solutions to area lights.
  It seems the representative point should only be used for specular contribution.
  Which point should be picked for diffuse contribution then?
  * Apparently the solution is supposed to be exact for sphere light diffuse
    (If no part of the light is occluded, which quickly becomes false for large spheres).
  * For other types of lights like tube and polygonal lights?
