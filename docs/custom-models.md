
# Custom hands, controllers and keyboard fitting (v0.5)

## Keyboard frame

Open **Keyboard → Measure with fingers**, then make the two L shapes shown in the reference: thumbs point inward and index fingers point toward the keyboard's far edge. Position the index fingers at the left/right edges and thumbs at the near edge. Keep both hands visible and steady for 1.2 seconds.

DeskHalo measures a rectangle in the hands' plane, including width, depth, position and rotation. A moving or incomplete frame resets the hold. The operation times out after 20 seconds. It is a finger-defined calibration, not camera recognition of a physical keyboard. Depth is the measured front-to-back distance, not the keyboard's thickness.

The measured width/depth are saved with profiles. The position is local to the current tracking origin: recentering resets the anchor, so repeat calibration after recentering. **Keyboard bounds → reset** restores the default dimensions and position. Keyboard height controls remain on the Appearance page.

## Import a model

Open **Appearance → Import OBJ · GLB · FBX**, choose one of four independent slots, and pick a file on the Quest. Files received through DeskHalo can be selected from Downloads/DeskHalo. Imported files stay in the app's private storage and reload at the next launch. Reset returns the selected slot to the built-in model.

Supported:
- Self-contained OBJ, GLB and FBX meshes.
- Material base colours and vertex colours.
- Embedded PNG/JPEG base-colour textures in GLB/FBX.
- Rigid controller models and rigid wrist-attached hand models.
- Weighted hand meshes using the OpenXR joint naming/axis convention below.

Limits: 32 MB per source file, 20,000 triangles and 128 mesh parts per model, at most eight embedded textures, each up to 2048×2048. External .mtl/texture references are not loaded. Export a self-contained GLB for textured assets. PBR lighting, normal maps, blend shapes and arbitrary animation clips are not implemented.

Export in metres with the wrist or controller grip at the origin, +Y up and forward toward -Z. Scale adjustment covers 25–400%. Rigid models can rotate around Y in 90-degree steps. Rigged hand orientation must be corrected in the authoring tool before export.

## Articulated hands

GLB/FBX hand rigs must use these bone names (case and separators are ignored; side prefixes are allowed):

- palm, wrist
- thumb_metacarpal, thumb_proximal, thumb_distal, thumb_tip
- index_metacarpal, index_proximal, index_intermediate, index_distal, index_tip
- middle_metacarpal, middle_proximal, middle_intermediate, middle_distal, middle_tip
- ring_metacarpal, ring_proximal, ring_intermediate, ring_distal, ring_tip
- little_metacarpal, little_proximal, little_intermediate, little_distal, little_tip

The bone-local bind axes must match OpenXR joint poses. Renaming an arbitrary rig alone does not retarget its axes. Unsupported bone names produce an import error; existing skins are retained. OBJ does not contain a skeletal rig and therefore follows the wrist rigidly.

## Controller feedback

Both controllers now read trigger, squeeze, face buttons and thumbstick input independently. The left menu button is represented; the system/Oculus button remains owned by the headset runtime.

Built-in controls highlight/depress while pressed and thumbsticks move with their axes. Custom mesh parts can be named trigger, squeeze, button_a/button_x, button_b/button_y, thumbstick/joystick or menu. These parts highlight and depress on activation. Generic indicator controls also remain visible above a custom model whose parts have no recognized names.

Controller surfaces use a depth buffer. Motion or button input restores full opacity; ten idle seconds starts a one-second fade to 10%. Last-known controller poses remain faintly visible when tracking disappears. Active hand tracking replaces that side's controller visual.

## Validation status

Pure-Java measurement tests cover dimensions, rotation, hold duration, invalid geometry, non-finite coordinates, movement and tracking loss. The Android/native build passes. The corrected APK was installed on Quest 2; OpenXR/passthrough initialized and the OBJ, GLB, FBX and invalid-file importer tests passed. Physical alignment and arbitrary custom-model visual checks remain pending.

Debug builds run non-mutating importer tests for OBJ, GLB, ASCII FBX and invalid GLB when entering VR. Look for "Model test ... PASS" in the DeskHalo log. These fixtures validate parser integration, not arbitrary custom rigs or physical alignment.

The update launcher writes DeskHalo-v0.5-update.log and DeskHalo-v0.5-quest.log beside the APK. A successful source compilation alone does not confirm in-headset alignment, button placement or gesture reliability.

## Bundled models in v0.6

Defaults use the MIT-licensed WebXR Input Profiles generic-hand and oculus-touch-v2 GLBs. Custom models still override each slot. Restore built-in appearance reloads the bundled asset. WebXR finger/phalanx/pinky bone labels map to the corresponding OpenXR joints. Face button names such as a_button and x_button are recognized. Generic press indicators are suppressed when a skin supplies named button meshes.
