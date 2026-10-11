// The demo skatepark, built from simple solids in skate space (metres, +Y up).
// Every piece returns triangles; the same soup goes to the skate library (collision + rail
// derivation) and to three.js (one mesh per piece).

export function buildPark() {
  const pieces = [];
  const piece = (name, color) => {
    const p = { name, color, positions: [] };
    pieces.push(p);
    return p;
  };
  const tri = (p, a, b, c) => p.positions.push(...a, ...b, ...c);
  const quad = (p, a, b, c, d) => { tri(p, a, b, c); tri(p, a, c, d); };

  // closed box between two corners, faces wound outward
  const box = (p, [x0, y0, z0], [x1, y1, z1]) => {
    quad(p, [x0, y1, z0], [x0, y1, z1], [x1, y1, z1], [x1, y1, z0]); // top
    quad(p, [x0, y0, z0], [x1, y0, z0], [x1, y0, z1], [x0, y0, z1]); // bottom
    quad(p, [x0, y0, z0], [x0, y0, z1], [x0, y1, z1], [x0, y1, z0]); // -x
    quad(p, [x1, y0, z0], [x1, y1, z0], [x1, y1, z1], [x1, y0, z1]); // +x
    quad(p, [x0, y0, z0], [x0, y1, z0], [x1, y1, z0], [x1, y0, z0]); // -z
    quad(p, [x0, y0, z1], [x1, y0, z1], [x1, y1, z1], [x0, y1, z1]); // +z
  };
  // ramp rising along +z from (z0, y=0) to (z1, y=h), x0..x1, with a vertical back and sides
  const wedge = (p, x0, x1, z0, z1, h) => {
    quad(p, [x0, 0, z0], [x0, h, z1], [x1, h, z1], [x1, 0, z0]);
    quad(p, [x0, 0, z1], [x1, 0, z1], [x1, h, z1], [x0, h, z1]);
    tri(p, [x0, 0, z0], [x0, 0, z1], [x0, h, z1]);
    tri(p, [x1, 0, z0], [x1, h, z1], [x1, 0, z1]);
  };
  // a curved quarter pipe facing -z: radius r, up to `angle` degrees, deck behind
  const quarter = (p, x0, x1, zStart, r, angleDeg, deck) => {
    const steps = 10;
    const a1 = (angleDeg * Math.PI) / 180;
    let prev = null;
    for (let i = 0; i <= steps; i++) {
      const a = (a1 * i) / steps;
      const z = zStart + r * Math.sin(a);
      const y = r - r * Math.cos(a);
      if (prev) quad(p, [x0, prev[1], prev[0]], [x0, y, z], [x1, y, z], [x1, prev[1], prev[0]]);
      prev = [z, y];
    }
    const [zl, yl] = prev;
    quad(p, [x0, yl, zl], [x0, yl, zl + deck], [x1, yl, zl + deck], [x1, yl, zl]); // deck
    quad(p, [x0, 0, zl + deck], [x1, 0, zl + deck], [x1, yl, zl + deck], [x0, yl, zl + deck]); // back
    // sides
    for (const x of [x0, x1]) {
      prev = null;
      for (let i = 0; i <= steps; i++) {
        const a = (a1 * i) / steps;
        const z = zStart + r * Math.sin(a);
        const y = r - r * Math.cos(a);
        if (prev) quad(p, [x, 0, prev[0]], [x, prev[1], prev[0]], [x, y, z], [x, 0, z]);
        prev = [z, y];
      }
      quad(p, [x, 0, zl], [x, yl, zl], [x, yl, zl + deck], [x, 0, zl + deck]);
    }
  };

  const ground = piece('ground', 0x2a2f36);
  quad(ground, [-60, 0, -60], [-60, 0, 60], [60, 0, 60], [60, 0, -60]);

  const walls = piece('walls', 0x3b4350);
  box(walls, [-60, 0, -61], [60, 2.5, -60]);
  box(walls, [-60, 0, 60], [60, 2.5, 61]);
  box(walls, [-61, 0, -60], [-60, 2.5, 60]);
  box(walls, [60, 0, -60], [61, 2.5, 60]);

  // ledges and a manual pad
  const concrete = piece('ledges', 0x8f9aa6);
  box(concrete, [-12, 0, 0], [-6, 0.45, 0.6]);
  box(concrete, [-12, 0, 6], [-4, 0.3, 8.5]);
  box(concrete, [6, 0, 10], [7, 0.55, 22]);

  // handrail on two posts: a thin bar (the rail derivation finds its middle)
  const rail = piece('rail', 0xd8b04a);
  box(rail, [-2.0, 0.55, 14], [-1.92, 0.63, 22]);
  box(rail, [-2.0, 0, 14.4], [-1.92, 0.55, 14.5]);
  box(rail, [-2.0, 0, 21.5], [-1.92, 0.55, 21.6]);

  // kicker and launch ramp
  const wood = piece('ramps', 0xb87b4b);
  wedge(wood, 2, 4.5, 2, 4, 0.55);
  wedge(wood, -4, -1, 28, 31, 1.0);
  box(wood, [-4, 0, 31], [-1, 1.0, 33]);

  // stairs with a hubba ledge beside them, going down along +z from a platform
  const stairs = piece('stairs', 0x7d8794);
  box(stairs, [12, 0, -14], [18, 1.2, -6]); // platform
  for (let i = 0; i < 4; i++) box(stairs, [12, 0, -6 + i * 0.6], [18, 1.2 - (i + 1) * 0.3 + 0.0001, -5.4 + i * 0.6]);
  const hubba = piece('hubba', 0xa3adb8);
  // sloped ledge: top goes from y=1.6 at z=-6 down to y=0.4 at z=-3.6
  const hx0 = 18, hx1 = 18.5;
  quad(hubba, [hx0, 1.6, -6], [hx0, 0.4, -3.6], [hx1, 0.4, -3.6], [hx1, 1.6, -6]);
  quad(hubba, [hx0, 0, -6], [hx1, 0, -6], [hx1, 0, -3.6], [hx0, 0, -3.6]);
  quad(hubba, [hx0, 0, -6], [hx0, 0, -3.6], [hx0, 0.4, -3.6], [hx0, 1.6, -6]);
  quad(hubba, [hx1, 0, -6], [hx1, 1.6, -6], [hx1, 0.4, -3.6], [hx1, 0, -3.6]);
  quad(hubba, [hx0, 0, -6], [hx0, 1.6, -6], [hx1, 1.6, -6], [hx1, 0, -6]);
  quad(hubba, [hx0, 0, -3.6], [hx1, 0, -3.6], [hx1, 0.4, -3.6], [hx0, 0.4, -3.6]);

  // funbox: banks up to a flat top, a flat bar across it
  const fun = piece('funbox', 0x5c7c9c);
  const f = { x0: -18, x1: -10, z0: 16, z1: 24, h: 0.6, run: 1.2 };
  const t = { x0: f.x0 + f.run, x1: f.x1 - f.run, z0: f.z0 + f.run, z1: f.z1 - f.run };
  quad(fun, [t.x0, f.h, t.z0], [t.x0, f.h, t.z1], [t.x1, f.h, t.z1], [t.x1, f.h, t.z0]);
  quad(fun, [f.x0, 0, f.z0], [t.x0, f.h, t.z0], [t.x1, f.h, t.z0], [f.x1, 0, f.z0]);
  quad(fun, [f.x1, 0, f.z1], [t.x1, f.h, t.z1], [t.x0, f.h, t.z1], [f.x0, 0, f.z1]);
  quad(fun, [f.x0, 0, f.z1], [t.x0, f.h, t.z1], [t.x0, f.h, t.z0], [f.x0, 0, f.z0]);
  quad(fun, [f.x1, 0, f.z0], [t.x1, f.h, t.z0], [t.x1, f.h, t.z1], [f.x1, 0, f.z1]);
  const bar = piece('flatbar', 0xd8b04a);
  box(bar, [-14.04, f.h + 0.3, t.z0 - 2], [-13.96, f.h + 0.38, t.z1 + 2]);
  box(bar, [-14.04, f.h, t.z0], [-13.96, f.h + 0.3, t.z0 + 0.08]);
  box(bar, [-14.04, f.h, t.z1 - 0.08], [-13.96, f.h + 0.3, t.z1]);

  // quarter pipes at both ends of the main run
  const qp = piece('quarters', 0x9c6b4a);
  quarter(qp, -6, 6, 40, 3.0, 52, 2.0);

  // gather: positions + indices for the library, one geometry per piece for drawing
  const vertices = [];
  const indices = [];
  for (const p of pieces) {
    for (let i = 0; i < p.positions.length; i += 3) {
      indices.push(vertices.length / 3);
      vertices.push(p.positions[i], p.positions[i + 1], p.positions[i + 2]);
    }
  }
  return { pieces, vertices: new Float32Array(vertices), indices: new Int32Array(indices), spawn: [0, 0.3, -8], heading: 0 };
}
