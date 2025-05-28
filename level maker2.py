import argparse

def parse_obj(filename):
    """Parse vertices and faces from a .obj file."""
    vertices = []
    faces = []  # list of faces (each face is a list of vertex indices)
    with open(filename, 'r') as f:
        for line in f:
            if not line or line.startswith('#'):
                continue  # skip empty lines and comments
            parts = line.strip().split()
            if len(parts) < 1:
                continue
            if parts[0] == 'v':
                # Vertex line: v x y z
                if len(parts) >= 4:
                    try:
                        x = float(parts[1]); y = float(parts[2]); z = float(parts[3])
                    except ValueError:
                        continue  # skip malformed vertex lines
                    vertices.append((x, y, z))
            elif parts[0] == 'f':
                # Face line: f v1 v2 v3 ... (vertices can be separated by '/', we take the first part as index)
                if len(parts) < 4:
                    continue  # not enough vertices to form a face
                # Extract vertex indices (OBJ indices are 1-based; they may include texture/normal indices separated by '/')
                indices = []
                for v in parts[1:]:
                    # Each face vertex token might look like "v_index", "v_index/t_index", or "v_index/t_index/n_index"
                    if not v:
                        continue
                    # Take the vertex index before any '/'
                    v_index_str = v.split('/')[0]
                    try:
                        vi = int(v_index_str)
                    except ValueError:
                        continue  # skip if not an integer
                    if vi < 0:
                        # Negative indices in OBJ refer to relative positions from the end of the vertex list
                        vi = len(vertices) + vi + 1
                    indices.append(vi)
                if len(indices) < 3:
                    continue  # skip degenerate face
                # Triangulate if face has more than 3 vertices (fan method)
                if len(indices) == 3:
                    faces.append(indices)
                else:
                    # break n-gon into triangles (v0,v[i],v[i+1])
                    for i in range(1, len(indices)-1):
                        tri = [indices[0], indices[i], indices[i+1]]
                        faces.append(tri)
    return vertices, faces

def generate_level_c(vertices, faces, scale=100.0, surf_type="SURFACE_DEFAULT", terrain="TERRAIN_GRASS"):
    """Generate the C source text for the level, given vertices and triangular faces."""
    lines = []
    # Header includes (adjust the include path as needed for your project structure)
    lines.append('#include "level.h"')
    lines.append('#include "../src/decomp/include/surface_terrains.h"')
    # Begin surfaces array
    lines.append("const struct SM64Surface surfaces[] = {")
    # Process each face (triangle)
    for face_index, face in enumerate(faces):
        # face is a list of three vertex indices (1-based index as per OBJ)
        # Retrieve the coordinates of each vertex and apply transformations
        verts = []
        for vi in face:
            # OBJ indices are 1-based; ensure index is valid:
            if vi < 1 or vi > len(vertices):
                # If out-of-bounds, skip or assign a default (should not happen with well-formed OBJ)
                vx = vy = vz = 0
            else:
                x, y, z = vertices[vi - 1]  # get vertex (OBJ -> Python index)
                # Apply coordinate transformation:
                # Blender (x,y,z) -> SM64 (X = x, Y = z, Z = -y), with scaling
                new_x = scale * x
                new_y = scale * z
                new_z = scale * (-y)
                # Convert to integer and clamp to 16-bit range
                # (SM64 collision coordinates are typically 16-bit signed)
                try:
                    vx = int(new_x)
                except OverflowError:
                    vx = int(new_x // 1)  # fallback in case of very large values
                try:
                    vy = int(new_y)
                except OverflowError:
                    vy = int(new_y // 1)
                try:
                    vz = int(new_z)
                except OverflowError:
                    vz = int(new_z // 1)
                # Clamp to [-32767, 32767]
                if vx < -0x7FFF: vx = -0x7FFF
                if vx >  0x7FFF: vx =  0x7FFF
                if vy < -0x7FFF: vy = -0x7FFF
                if vy >  0x7FFF: vy =  0x7FFF
                if vz < -0x7FFF: vz = -0x7FFF
                if vz >  0x7FFF: vz =  0x7FFF
            verts.append((vx, vy, vz))
        # Construct the surface entry line
        # Format: {SURFACE_TYPE, 0, TERRAIN_TYPE, {{x1,y1,z1},{x2,y2,z2},{x3,y3,z3}}}
        v0, v1, v2 = verts[0], verts[1], verts[2]
        line = f"    {{{surf_type},0,{terrain},{{{{{v0[0]},{v0[1]},{v0[2]}}},{{{v1[0]},{v1[1]},{v1[2]}}},{{{v2[0]},{v2[1]},{v2[2]}}}}}}}"
        # Add comma except for the last surface (optional – C99 allows a trailing comma)
        if face_index != len(faces) - 1:
            line += ","
        lines.append(line)
    lines.append("};")
    # surfaces_count for convenience
    lines.append("const size_t surfaces_count = sizeof(surfaces) / sizeof(surfaces[0]);")
    return "\n".join(lines)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert a .obj model to libSM64 level collision C code.")
    parser.add_argument("input_obj", help="Path to the input .obj file")
    parser.add_argument("output_c", help="Path for the output C source file (e.g., level.c)")
    parser.add_argument("--scale", type=float, default=100.0, help="Scaling factor from Blender units to SM64 units (default: 100)")
    args = parser.parse_args()
    verts, faces = parse_obj(args.input_obj)
    if not faces:
        raise RuntimeError("No faces found in OBJ file – ensure the model has triangulated faces.")
    level_c_content = generate_level_c(verts, faces, scale=args.scale)
    # Write to output file
    with open(args.output_c, 'w') as f:
        f.write(level_c_content)
    print(f"Successfully generated {args.output_c} with {len(faces)} surfaces.")
