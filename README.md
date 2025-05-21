# Real-Time Rendering Prototypes

Prototypes developed while reading RTR 4th edition.

## Usage

Clone [`realtime_rendering_assets`](https://github.com/Adnn/realtime_rendering_assets) **next** to this repository:

```bash
git clone git@github.com:Adnn/realtime_rendering_assets.git assets
```

## Content

The repository contains several standalone applications demonstrating real-time rendering techniques.

### ch11_ssao

Implement Screen-Space Ambient Occlusion (SSAO) in a viewer supporting PBR shading and image-based lighting (IBL).
The scene is highly customizable via a GUI.

Two SSAO variants are provided, both with optional bilateral filtering:
* Spherical sampling (_Crytek_).
* [Normal-oriented hemispherical sampling](https://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html), with optional importance sampling.

[![ssao_ibl_split](https://adnn.github.io/assets/rtr_prototypes/ch11_ssao/rtr_11-diagonal-ao_shade-864.png)](https://adnn.github.io/assets/rtr_prototypes/ch11_ssao/rtr_11-diagonal_ao_shade-1920.jpg)


### ch10_ltc

Implement [Linearly Transformed Cosines](https://eheitzresearch.wordpress.com/415-2/) (LTC) for shading with polygonal-light.\

LTCs accurately approximate common BRDFs, such as GGX.
The integration of an LTC with a polygonal light is analytic and exact.
Computational cost scales linearly with the number of light edges.

[![ltc_colored_lights-clay](https://adnn.github.io/assets/rtr_prototypes/ch10_ltc/colored_scene-clay-864.png)](https://adnn.github.io/assets/rtr_prototypes/ch10_ltc/colored_scene-clay.jpg)

### ch10_area_lights

Implement sphere lights and tube lights with fast approximations, such as most-representative point.

[![area-lights roughness scale](https://adnn.github.io/assets/rtr_prototypes/ch10_area_lights/area_lights-roughness_variations-864_646.jpg)](https://adnn.github.io/assets/rtr_prototypes/ch10_area_lights/area_lights-roughness_variations.jpg)
