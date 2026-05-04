# Sample Images — Sources

All images in `data/images/` are **procedurally generated** by
`scripts/generate_sample_images.py`. None of them are downloaded, photographed,
or sourced from third parties.

This means:

- They are **100% original and CC0 / public domain**.
- They are **safe to commit, distribute, and use commercially**.
- You can regenerate them at any time by running `make sample-data` — every
  run produces the same images deterministically (the random seed is fixed in
  the script).

If you want to swap them out for real photographs, drop your own PNG/JPG files
into `data/images/`. The Python server converts new files to PPM on startup
and invalidates the feature cache automatically.

## Image catalog

| File                    | Description                          |
| ----------------------- | ------------------------------------ |
| 01_sunset_warm.png      | warm-toned stylized sunset landscape |
| 02_sunset_purple.png    | purple-to-orange diagonal gradient   |
| 03_dawn_soft.png        | soft dawn landscape                  |
| 04_ocean_blue.png       | ocean-style vertical gradient        |
| 05_ocean_deep.png       | deep navy gradient                   |
| 06_forest_green.png     | stylized forest scene                |
| 07_forest_dark.png      | dark green vertical gradient         |
| 08_desert_warm.png      | warm desert landscape                |
| 09_radial_violet.png    | violet radial gradient               |
| 10_radial_amber.png     | amber radial gradient                |
| 11_stripes_navy.png     | navy horizontal stripes              |
| 12_stripes_crimson.png  | crimson vertical stripes             |
| 13_dotted_mint.png      | mint-on-mint dotted pattern          |
| 14_dotted_rose.png      | rose-on-pink dotted pattern          |
| 15_checker_mono.png     | monochrome checkerboard              |
| 16_checker_warm.png     | warm-toned checkerboard              |
| 17_gradient_cyan.png    | cyan diagonal gradient               |
| 18_gradient_pink.png    | pink diagonal gradient               |
| 19_gradient_lime.png    | lime diagonal gradient               |
| 20_gradient_dusk.png    | dusk-colored diagonal gradient       |
