/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_task.hh"

#include "BKE_material.h"

namespace blender::nodes::node_geo_legacy_select_by_material_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Material>(N_("Material")).hide_label();
  b.add_input<decl::String>(N_("Selection"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void select_mesh_by_material(const Mesh &mesh,
                                    const Material *material,
                                    const MutableSpan<bool> r_selection)
{
  BLI_assert(mesh.totpoly == r_selection.size());
  Vector<int> material_indices;
  for (const int i : IndexRange(mesh.totcol)) {
    if (mesh.mat[i] == material) {
      material_indices.append(i);
    }
  }
  threading::parallel_for(r_selection.index_range(), 1024, [&](IndexRange range) {
    for (const int i : range) {
      r_selection[i] = material_indices.contains(mesh.mpoly[i].mat_nr);
    }
  });
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Material *material = params.extract_input<Material *>("Material");
  const std::string selection_name = params.extract_input<std::string>("Selection");

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  geometry_set = geometry::realize_instances_legacy(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
    const Mesh *mesh = mesh_component.get_for_read();
    if (mesh != nullptr) {
      OutputAttribute_Typed<bool> selection =
          mesh_component.attribute_try_get_for_output_only<bool>(selection_name, ATTR_DOMAIN_FACE);
      if (selection) {
        select_mesh_by_material(*mesh, material, selection.as_span());
        selection.save();
      }
    }
  }

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_legacy_select_by_material_cc

void register_node_type_geo_legacy_select_by_material()
{
  namespace file_ns = blender::nodes::node_geo_legacy_select_by_material_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_SELECT_BY_MATERIAL, "Select by Material", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
