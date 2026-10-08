# Solar System Visualization

A real-time 3D simulation written in C++ with OpenGL and FreeGLUT. It shows the Sun, all eight planets, and Earth's Moon on circular orbits. Planet and Sun sizes are true relative to each other, and orbital distances are scaled down so the system is navigable. It also demonstrates how the Moon's phases come from lighting geometry.

## Features

- Sun at the origin, acting as the point light source for the whole scene
- Eight planets (Mercury to Neptune) drawn from a single data table by a single loop in `display()`
- Every planet revolves around the Sun, with speed following Kepler's third law (closer planets move faster)
- Saturn has a tilted ring (1.24 to 2.27 Saturn radii, tilted 26.7°), lit on both sides
- Faint, planet-coloured orbit lines in both modes
- Debug readout (`B`) showing the camera position and movement speed
- Earth spins on its axis; the Moon orbits Earth and is tidally locked (one rotation per orbit, the same face toward Earth)
- Textured Sun, planets, Moon and Saturn's ring, with a Milky Way star background (see Textures)
- Moon phases come out of the lighting itself: Phong illumination with per-vertex normals, no phase textures
- Free-fly camera (keyboard and mouse) and a Moon-lock camera that sits on the Earth–Moon line looking at the Moon
- **Overview mode (default):** the whole solar system fits on screen, with accurate relative sizes (the Sun is far larger than any planet) and compressed orbit distances
- **True-scale mode:** orbit distances are proportional to real AU values (set by the `AU` constant); press `O` to switch
- Planet jump keys, because the true-scale system is too large to fly across by hand
- Custom sphere mesh (stacks × slices triangles), no `gluSphere()`
- Depth-buffer visible surface determination, perspective projection, about 60 FPS animation

## Build and Run

You need a C++ compiler plus OpenGL and FreeGLUT. The image loader `stb_image.h` is included in the repository, so there is nothing else to install. Run the program from the folder that contains the `textures/` folder (see Textures).

**Windows (MinGW):**

```
g++ Solar_System.cpp -o Solar_System -lfreeglut -lopengl32 -lglu32
Solar_System.exe
```

If FreeGLUT is not on the default search path, add `-I"<freeglut>/include" -L"<freeglut>/lib"`, and keep `freeglut.dll` next to the executable.

**Linux:**

```
sudo apt install freeglut3-dev
g++ Solar_System.cpp -o Solar_System -lglut -lGLU -lGL
./Solar_System
```

**macOS:**

```
g++ Solar_System.cpp -o Solar_System -framework OpenGL -framework GLUT
```

## Textures

Put these images in a folder named `textures` next to where you run the program:

| File | Used for |
|---|---|
| `sun.jpg` | Sun |
| `mercury.jpg` | Mercury |
| `venus_surface.jpg` | Venus |
| `earth.jpg` | Earth |
| `moon.jpg` | Moon |
| `mars.jpg` | Mars |
| `jupiter.jpg` | Jupiter |
| `saturn.jpg` | Saturn |
| `saturn_ring_alpha.png` | Saturn's ring (with transparency) |
| `uranus.jpg` | Uranus |
| `neptune.jpg` | Neptune |
| `stars_milky_way.jpg` | Star background (a large sphere around the camera) |

Any missing file is reported in the console and that body is drawn in a plain colour instead (a missing star background is simply black), so the program still runs. The 2k versions of the images are recommended, because larger ones take longer to load at start-up.

Sphere textures are equirectangular maps (longitude left to right, latitude top to bottom). The ring image is read as a radial strip: its left edge is the inner edge of the ring and its right edge is the outer edge.

### Credits

- Textures: [Solar System Scope](https://www.solarsystemscope.com/textures/), licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
- Image loading: [`stb_image.h`](https://github.com/nothings/stb) by Sean Barrett, public domain.

## Controls

| Key | Action |
|---|---|
| `W` `A` `S` `D` | Move forward / left / back / right relative to where you look, staying level (height never changes) |
| Mouse | Look around |
| `Space` / `Shift` | Move up / down (holding `Shift` alone needs FreeGLUT to report the Shift key) |
| `O` | Toggle Overview mode (default) / True-scale mode |
| `R` or `0` | Reset to the top-down view above the Sun |
| `1`–`8` | Jump above Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune, stopping at the minimum camera distance |
| `M` | Toggle Moon-lock camera |
| `+` / `-` | Move the Moon-lock camera closer to / farther from the Moon |
| `P` | Pause / resume the animation |
| `B` | Toggle the debug readout: camera position and movement speed (top-left) |
| `Esc` | Exit |

In Moon-lock mode only `M`, `P`, `B`, `+`, `-` and `Esc` are active, so press `M` before using the jump keys or `O`. Movement keys are tracked while held and applied every frame, so movement is smooth rather than dependent on the operating system's key repeat. Movement speed adapts to distance (see Approaching Planets) and is the same whether or not the animation is paused.

## Approaching Planets

- **Stop distance:** the camera can't get closer than 3 radii from a planet's centre (2 radii above its surface), and 1.2 Sun radii from the Sun. If you move into that distance, or a planet's orbit carries it into you, the camera is pushed back out, so you never pass through a body. In Moon-lock mode this is not applied.
- **Slowdown:** revolution speed slows down gradually as the camera approaches a planet. The planets' orbits are at full speed 40 radii away and fall to 2% of normal at the stop distance. The Moon's orbit slows by the same rule but never below half speed, so it stays visibly moving when you are near Earth. Planet spin is not affected, and the slowdown is switched off in Moon-lock mode so the Moon keeps cycling through its phases.
- **Gradual approach:** each movement keypress covers 10% of the distance to the nearest body's surface (planets and the Sun). Steps are large in open space and shrink automatically as you close in, so you can settle at the stop distance without overshooting.
- **Jump keys:** `1`–`8` place the camera directly above the planet at exactly the stop distance.

The constants `STOP_MULT`, `SLOW_MULT`, `MIN_TIME` and `SUN_STOP` at the top of the code control these.

## Scale

There are two modes. Planet and Sun **sizes are the same in both and are accurate relative to each other**; only the orbit distances differ.

### Sizes (both modes)

Units are Mercury radii (Mercury radius = 1). Earth's radius is therefore 2.611 units.

| Body | Radius | Distance from Sun |
|---|---|---|
| Sun | 285.1 | 0 |
| Mercury | 1.000 | 0.387 AU |
| Venus | 2.478 | 0.723 AU |
| Earth | 2.611 | 1.000 AU |
| Mars | 1.389 | 1.524 AU |
| Jupiter | 29.27 | 5.203 AU |
| Saturn | 24.67 | 9.58 AU |
| Uranus | 10.47 | 19.2 AU |
| Neptune | 10.13 | 30.1 AU |

### True-scale mode (`O` to select)

1 AU is set to 2,611 units by the `AU` constant at the top of the code. The true Sun–Earth distance would be about 61,240 units, so distances here are roughly 23 times compressed relative to the sizes. Lower `AU` to shrink the system further; it must stay above about 737 so Mercury clears the Sun. The planets are still small compared with the distances between them, so from most viewpoints they are not visible. Use the number keys to jump to one.

### Overview mode (default)

Orbit radius is `246.6 × (1 + √AU)`, so Mercury orbits at about 400 units (just outside the Sun's 285-unit radius) and Neptune at about 1,600. The camera starts 3,000 units above the Sun looking straight down, so every orbit is in view. Planet speeds still follow Kepler's law using the true distances.

At this zoom most planets are smaller than a pixel, so each planet is also marked with a small coloured dot. The spheres themselves keep their accurate sizes, and you can use `1`–`8` to jump close to any planet.

## How It Works

The scene follows the standard 3D graphics pipeline.

1. **Modeling:** spheres are generated as triangle meshes. A vertex at stack angle θ and slice angle φ is `(r·sinθ·cosφ, r·cosθ, r·sinθ·sinφ)`, and its normal is the vertex divided by `r`.
2. **World placement:** each planet is placed on a circular orbit around the Sun with `getPlanetPos(distance)`. The Moon's position is Earth's position plus an orbit of radius 7.83 units (3 Earth radii).
3. **Viewing:** `gluLookAt()` uses either the free camera (yaw/pitch angles) or the Moon-lock camera.
4. **Projection:** `gluPerspective(60°, aspect, 0.26, 261,000)`.
5. **Illumination:** OpenGL's Phong-style lighting with a point light at the Sun's position (ambient 0.2, diffuse 0.9, specular 1.0). The Sun itself is drawn unlit so it looks self-illuminated.
6. **Visibility:** the depth buffer (`GL_DEPTH_TEST`) is cleared every frame, so the Moon correctly passes in front of and behind Earth.
7. **Animation:** a 16 ms `glutTimerFunc` advances the angles each frame.

### Why the phases appear

A phase depends only on the angle between the Sun, Moon and viewer. Each Moon vertex is lit when its normal faces the Sun, so as the Moon orbits, the lit hemisphere seen from Earth changes from new to full and back. The Moon is also tidally locked and spins once per orbit, so the same face of its texture always points at Earth. The spin does not change the phases.

### Animation rates (per frame)

| Motion | Rate |
|---|---|
| Moon orbit around Earth | −0.5° |
| Earth spin | 2° |
| Earth orbit around Sun | −0.04° |
| Other planets' orbits | Earth's rate × (Earth distance / distance)^1.5 |

All planets spin at Earth's rate.

## Code Overview

Everything is in `Solar_System.cpp`, which has thirteen functions:

| Function | Purpose |
|---|---|
| `getPlanetPos()` | Position of a planet on its circular orbit (Kepler speed, true or compressed radius) |
| `loadTexture()` | Loads an image from `textures/` into an OpenGL texture with mipmaps |
| `drawSphere()` | Triangle-mesh sphere with per-vertex normals and texture coordinates |
| `physics()` | Per-frame camera and time rules: stop distance around bodies, gradual approach step size, and slowdown of revolutions near planets |
| `display()` | Camera, light, star background, planets, Moon, Sun, Saturn's ring, orbit lines, overview dots, controls overlay |
| `keyboard()` / `keyboardUp()` | Key presses and releases: one-shot actions (reset, overview toggle, planet jumps, Moon lock, pause) and the held-key table |
| `special()` / `specialUp()` | Track whether Shift is held (it does not auto-repeat) |
| `mouseMotion()` | Mouse look |
| `update()` | Animation timer: applies held-key movement, the stop distance and the orbit and spin angles each frame |
| `reshape()` | Window resize and perspective projection |
| `main()` | Window, lighting and OpenGL setup, then the main loop |

To add or change a planet, edit its row in the `planets[]` table: radius, distance, and RGB colour.

## Limitations

- Orbital distances are compressed in both modes: square-root compression in Overview mode, and about 23 times in True-scale mode, relative to the planet sizes.
- In Overview mode the planet dots are markers, not to scale.
- The Moon's orbit radius is 3 Earth radii (7.83 units) for visibility. The true value is about 60 Earth radii.
- Saturn casts no shadow on its ring, and the ring does not receive one from the planet. No moons for other planets, planetary axial tilts (other than the ring's), or elliptical orbits.
- Every planet spins at the same rate.
- At the true scale, very distant objects lose depth precision, so planets seen from far away can show depth artifacts.

## Future Work

- Earth's axial tilt (23.5°) and elliptical orbits
- Bump or normal maps, cloud layers for Earth and Venus, and night-side city lights