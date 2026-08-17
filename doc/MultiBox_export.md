# Multi-box PNG export (Phase 1)

PrusaSlicer can export **static multi-box projection PNGs** compatible with the MATLAB `RotarySegmentation` workflow and the CLIP/Qt GUI.

## Workflow

1. Load an STL and slice the plate (FFF).
2. Open **Print settings → Output options → Multi-box export** and configure box parameters.
3. Use **File → Export → Export Multi-box PNGs…**
4. Choose an output folder and project name. Export runs on a background thread with progress in the status bar.

## Output format

Files are written as:

```text
<project>_<layer4d>_<box4d>.png
```

Example for 4 boxes and layer 5:

```text
MyPart_0005_0001.png
MyPart_0005_0002.png
MyPart_0005_0003.png
MyPart_0005_0004.png
```

Default image size: **2560×1600** pixels (configurable).

## Settings (MATLAB defaults)

| Setting | Default | MATLAB equivalent |
|---------|---------|-------------------|
| Number of boxes | 4 | `numBoxes` |
| Angle between boxes | 90° | `angleBetweenBoxes` |
| Starting angle | 0° | `startingAngle` |
| Box width / height | 2560 / 1600 px | `boxWidth` / `boxHeight` |
| Pixel scale | 3.78 µm/px | `Scalepixels` |
| Fabrication area | 12.8 × 12.8 mm | `MaxWidth` / `MaxHeight` |
| Box radius | 4.76 mm | calibration curve (constant in Phase 1) |

## Coordinate handling

Layer contours are translated so the **model center** maps to the **fabrication area center**, matching MATLAB `centerSTLObject` / `translateSlicesToCenter`. Each box clips and rasterizes its portion of the slice in box-local coordinates.

## Validation checklist

Use [`50um_grilled circle.stl`](../Matlab/Jize%20Dai's%20files%20-%20slicer/STL/50um_grilled%20circle.stl) with the same settings in MATLAB and PrusaSlicer:

- [ ] File count = `num_layers × num_boxes`
- [ ] Naming matches `Project_LLLL_BBBB.png`
- [ ] Image dimensions match configured box width/height
- [ ] Visual comparison vs MATLAB export (allow minor anti-aliasing differences)
- [ ] Box index `i` corresponds to angle `startingAngle + i × angleBetweenBoxes`

## Phase 1 limitations

- **Static boxing only** (fixed box count and angles per layer)
- **First print object only** when multiple objects are on the plate
- Constant box radius (no angle→radius calibration table yet)
- No dynamic ILP boxing, corkscrew mode, overlap greyscaling, vignette masks, or motion script CSV

## Implementation

- Core: `src/libslic3r/MultiBox/MultiBoxExporter.{hpp,cpp}`
- Background job: `src/slic3r/GUI/Jobs/MultiBoxExportJob.{hpp,cpp}`
- Config: `multibox_*` keys in `PrintConfig`
- GUI: **File → Export → Export Multi-box PNGs…**

## Build and manual validation

Build PrusaSlicer per [How to build - Linux et al.md](How%20to%20build%20-%20Linux%20et%20al.md), then:

1. Load `Slicing/Matlab/Jize Dai's files - slicer/STL/50um_grilled circle.stl`
2. Use default multi-box settings (4 boxes, 90° spacing, 2560×1600, 3.78 µm/px)
3. Slice and export multi-box PNGs
4. Compare file count, naming, dimensions, and visuals against MATLAB export with the same parameters
