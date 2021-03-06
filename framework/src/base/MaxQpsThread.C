/****************************************************************/
/*               DO NOT MODIFY THIS HEADER                      */
/* MOOSE - Multiphysics Object Oriented Simulation Environment  */
/*                                                              */
/*           (c) 2010 Battelle Energy Alliance, LLC             */
/*                   ALL RIGHTS RESERVED                        */
/*                                                              */
/*          Prepared by Battelle Energy Alliance, LLC           */
/*            Under Contract No. DE-AC07-05ID14517              */
/*            With the U. S. Department of Energy               */
/*                                                              */
/*            See COPYRIGHT for full restrictions               */
/****************************************************************/

#include "MaxQpsThread.h"

#include "FEProblem.h"

// libmesh includes
#include "libmesh/fe_base.h"
#include "libmesh/threads.h"
#include LIBMESH_INCLUDE_UNORDERED_SET
LIBMESH_DEFINE_HASH_POINTERS

MaxQpsThread::MaxQpsThread(FEProblem & fe_problem, QuadratureType qtype, Order order, Order face_order) :
    _fe_problem(fe_problem),
    _qtype(qtype),
    _order(order),
    _face_order(face_order),
    _max(0)
{
}

// Splitting Constructor
MaxQpsThread::MaxQpsThread(MaxQpsThread & x, Threads::split /*split*/) :
    _fe_problem(x._fe_problem),
    _qtype(x._qtype),
    _order(x._order),
    _face_order(x._face_order),
    _max(x._max)
{
}

void
MaxQpsThread::operator() (const ConstElemRange & range)
{
  ParallelUniqueId puid;
  _tid = puid.id;

  // For short circuiting reinit
  std::set<ElemType> seen_it;
  for (ConstElemRange::const_iterator elem_it = range.begin() ; elem_it != range.end(); ++elem_it)
  {
    const Elem * elem = *elem_it;

    // Only reinit if the element type has not previously been seen
    if (seen_it.insert(elem->type()).second)
    {
      FEType fe_type(FIRST, LAGRANGE);
      unsigned int dim = elem->dim();
      unsigned int side = 0;           // we assume that any element will have at least one side ;)

      // We cannot mess with the FE objects in Assembly, because we might need to request second derivatives
      // later on. If we used them, we'd call reinit on them, thus making the call to request second
      // derivatives harmful (i.e. leading to segfaults/asserts). Thus, we have to use a locally allocated object here.
      FEBase * fe = FEBase::build(dim, fe_type).release();

      // figure out the number of qps for volume
      QBase * qrule = QBase::build(_qtype, dim, _order).release();
      fe->attach_quadrature_rule(qrule);
      fe->reinit(elem);
      if (qrule->n_points() > _max)
        _max = qrule->n_points();
      delete qrule;

      // figure out the number of qps for the face
      // NOTE: user might specify higher order rule for faces, thus possibly ending up with more qps than in the volume
      QBase * qrule_face = QBase::build(_qtype, dim - 1, _face_order).release();
      fe->attach_quadrature_rule(qrule_face);
      fe->reinit(elem, side);
      if (qrule_face->n_points() > _max)
        _max = qrule_face->n_points();
      delete qrule_face;

      delete fe;
    }
  }
}

void
MaxQpsThread::join(const MaxQpsThread & y)
{
  if (y._max > _max)
    _max = y._max;
}
