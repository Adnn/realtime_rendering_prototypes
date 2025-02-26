VS:
* input prefix a_, or ve_ (per vertex) / in_ (per instance). To decide
* output: ex_(if followed by FS) or vs_(if followed by another stage)

Tessellation:
* input `vs_`
* output `ex_`

FS:
* input: `ex_`

Uniforms: `u_`

Uniform buffers: Type name ends in `Block`, variables prefixed by `ub_`

Suffixes for spaces:
* `_tbn` (tangent space)
* `_local`
* `_world`
* `_view`
* `_clip` (homoegenous 4D space)
* `_ndc` (after perspective divide)

For texture coordinates:
* uv when normalized texture coordinates
* texel when integer texture coordinates
