// =========================================================================
// @author Leonardo Florez-Valencia (florez-l@javeriana.edu.co)
// =========================================================================

#include <plugins/DotPPScene/ParametricSurfaceBuilder.h>
#include <pujOgre/vtkParametricExpression.h>

#include <pugixml.hpp>

#include <vtkParametricEllipsoid.h>
#include <vtkParametricFunctionSource.h>
#include <vtkParametricSuperEllipsoid.h>
#include <vtkParametricTorus.h>
#include <vtkPolyData.h>

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>

namespace
{
  using ParamMap = std::map< std::string, double >;

  double param(
    const ParamMap& params, const std::string& name, double defaultValue
    )
  {
    auto it = params.find( name );
    return( ( it != params.end( ) )? it->second: defaultValue );
  }

  void applyRange(
    const pugi::xml_node& range, vtkParametricFunction* fn, bool isU
    )
  {
    if( !range )
      return;

    if( isU )
    {
      fn->SetMinimumU(range.attribute("min").as_double(fn->GetMinimumU()));
      fn->SetMaximumU(range.attribute("max").as_double(fn->GetMaximumU()));
      if( range.attribute("join"))
        fn->SetJoinU(range.attribute("join").as_bool());
    }
    else
    {
      fn->SetMinimumV(range.attribute("min").as_double(fn->GetMinimumV()));
      fn->SetMaximumV(range.attribute("max").as_double(fn->GetMaximumV()));
      if (range.attribute("join"))
        fn->SetJoinV(range.attribute("join").as_bool());
    }
  }
} // namespace

vtkSmartPointer< vtkPolyData > buildParametricPolyData(
  const pugi::xml_node& xml
  )
{
  const std::string type = xml.attribute("type").as_string("expression");

  ParamMap params;
  for (auto p : xml.children("param"))
  {
    const std::string name = p.attribute("name").as_string();
    if (name.empty() || !p.attribute("value"))
      throw std::runtime_error("<param> needs both 'name' and 'value'");
    params[name] = p.attribute("value").as_double();
  }

  vtkSmartPointer<vtkParametricFunction> fn;

  if (type == "expression")
  {
    auto expr = vtkSmartPointer< pujOgre::vtkParametricExpression >::New();
    for (const auto& p : params)
      expr->SetConstant(p.first, p.second);

    auto point = xml.child("point");
    if (!point)
      throw std::runtime_error("type=\"expression\" needs a <point x=\"...\" y=\"...\" z=\"...\"/> element");
    expr->SetPointExpressions(point.attribute("x").as_string(), point.attribute("y").as_string(),
                              point.attribute("z").as_string());

    if (auto normal = xml.child("normal"))
      expr->SetNormalExpressions(normal.attribute("x").as_string(), normal.attribute("y").as_string(),
                                 normal.attribute("z").as_string());

    expr->Compile(); // report expression errors now, not in the middle of the VTK pipeline
    fn = expr;
  }
  else if (type == "torus")
  {
    auto torus = vtkSmartPointer<vtkParametricTorus>::New();
    torus->SetRingRadius(param(params, "ringRadius", 1.0));
    torus->SetCrossSectionRadius(param(params, "crossSectionRadius", 0.5));
    fn = torus;
  }
  else if (type == "ellipsoid")
  {
    auto ellipsoid = vtkSmartPointer<vtkParametricEllipsoid>::New();
    ellipsoid->SetXRadius(param(params, "xRadius", 1.0));
    ellipsoid->SetYRadius(param(params, "yRadius", 1.0));
    ellipsoid->SetZRadius(param(params, "zRadius", 1.0));
    fn = ellipsoid;
  }
  else if (type == "superEllipsoid")
  {
    auto se = vtkSmartPointer<vtkParametricSuperEllipsoid>::New();
    se->SetXRadius(param(params, "xRadius", 1.0));
    se->SetYRadius(param(params, "yRadius", 1.0));
    se->SetZRadius(param(params, "zRadius", 1.0));
    se->SetN1(param(params, "n1", 1.0));
    se->SetN2(param(params, "n2", 1.0));
    fn = se;
  }
  else
  {
    throw std::runtime_error("unknown parametricSurface type '" + type + "'");
  }

  applyRange(xml.child("uRange"), fn, true);
  applyRange(xml.child("vRange"), fn, false);

  // Swap which side of the surface is the front.
  if (xml.attribute("flipWinding").as_bool(false))
    fn->SetClockwiseOrdering(!fn->GetClockwiseOrdering());

  auto source = vtkSmartPointer<vtkParametricFunctionSource>::New();
  source->SetParametricFunction(fn);
  source->SetUResolution(std::max(2, xml.attribute("uResolution").as_int(50)));
  source->SetVResolution(std::max(2, xml.attribute("vResolution").as_int(50)));
  source->SetGenerateTextureCoordinates(xml.attribute("textureCoordinates").as_bool(true));
  source->GenerateNormalsOn();
  source->SetScalarModeToNone();
  source->Update();

  vtkSmartPointer<vtkPolyData> out = source->GetOutput();
  if (!out || out->GetNumberOfPoints() == 0)
    throw std::runtime_error("parametric surface produced no geometry");
  return out;
}

// eof - ParametricSurfaceBuilder.cxx
