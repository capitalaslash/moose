/****************************************************************/
/* MOOSE - Multiphysics Object Oriented Simulation Environment  */
/*                                                              */
/*          All contents are licensed under LGPL V2.1           */
/*             See LICENSE for full restrictions                */
/****************************************************************/
#include "StressDivergence.h"

#include "Material.h"
#include "SymmElasticityTensor.h"

template<>
InputParameters validParams<StressDivergence>()
{
  InputParameters params = validParams<Kernel>();
  params.addRequiredParam<unsigned int>("component", "An integer corresponding to the direction the variable this kernel acts in. (0 for x, 1 for y, 2 for z)");
  params.addCoupledVar("disp_x", "The x displacement");
  params.addCoupledVar("disp_y", "The y displacement");
  params.addCoupledVar("disp_z", "The z displacement");
  params.addCoupledVar("temp", "The temperature");
  params.addParam<Real>("zeta", 0, "zeta parameter");
  params.addParam<Real>("alpha", 0, "alpha parameter");
  params.addParam<std::string>("appended_property_name", "", "Name appended to material properties to make them unique");

  params.set<bool>("use_displaced_mesh") = true;

  return params;
}


StressDivergence::StressDivergence(const InputParameters & parameters) :
    Kernel(parameters),
    _stress_old(getMaterialPropertyOld<SymmTensor>("stress" + getParam<std::string>("appended_property_name"))),
    _stress(getMaterialProperty<SymmTensor>("stress" + getParam<std::string>("appended_property_name"))),
    _Jacobian_mult(getMaterialProperty<SymmElasticityTensor>("Jacobian_mult" + getParam<std::string>("appended_property_name"))),
    _d_stress_dT(getMaterialProperty<SymmTensor>("d_stress_dT"+ getParam<std::string>("appended_property_name"))),
    _component(getParam<unsigned int>("component")),
    _xdisp_coupled(isCoupled("disp_x")),
    _ydisp_coupled(isCoupled("disp_y")),
    _zdisp_coupled(isCoupled("disp_z")),
    _temp_coupled(isCoupled("temp")),
    _xdisp_var(_xdisp_coupled ? coupled("disp_x") : 0),
    _ydisp_var(_ydisp_coupled ? coupled("disp_y") : 0),
    _zdisp_var(_zdisp_coupled ? coupled("disp_z") : 0),
    _temp_var(_temp_coupled ? coupled("temp") : 0),
    _zeta(getParam<Real>("zeta")),
    _alpha(getParam<Real>("alpha"))
{}

Real
StressDivergence::computeQpResidual()
{
  if ((_dt > 0) && ((_zeta != 0) || (_alpha != 0)))
    return _stress[_qp].rowDot(_component, _grad_test[_i][_qp]) * (1 + _alpha + _zeta / _dt) - (_zeta / _dt + _alpha) * _stress_old[_qp].rowDot(_component, _grad_test[_i][_qp]);
  else
    return _stress[_qp].rowDot(_component, _grad_test[_i][_qp]);
}

Real
StressDivergence::computeQpJacobian()
{
  if (_dt > 0)
    return _Jacobian_mult[_qp].stiffness(_component, _component, _grad_test[_i][_qp], _grad_phi[_j][_qp]) * (1 + _alpha + _zeta/_dt);
  else
    return _Jacobian_mult[_qp].stiffness(_component, _component, _grad_test[_i][_qp], _grad_phi[_j][_qp]);
}

Real
StressDivergence::computeQpOffDiagJacobian(unsigned int jvar)
{
  unsigned int coupled_component = 0;

  bool active = false;

  if (_xdisp_coupled && jvar == _xdisp_var)
  {
    coupled_component = 0;
    active = true;
  }
  else if (_ydisp_coupled && jvar == _ydisp_var)
  {
    coupled_component = 1;
    active = true;
  }
  else if (_zdisp_coupled && jvar == _zdisp_var)
  {
    coupled_component = 2;
    active = true;
  }

  if (active)
  {
    if (_dt > 0)
      return _Jacobian_mult[_qp].stiffness(_component, coupled_component, _grad_test[_i][_qp], _grad_phi[_j][_qp]) * (1 + _alpha + _zeta / _dt);
    else
      return _Jacobian_mult[_qp].stiffness(_component, coupled_component, _grad_test[_i][_qp], _grad_phi[_j][_qp]);
  }

  if (_temp_coupled && jvar == _temp_var)
    return _d_stress_dT[_qp].rowDot(_component, _grad_test[_i][_qp]) * _phi[_j][_qp];

  return 0;
}
