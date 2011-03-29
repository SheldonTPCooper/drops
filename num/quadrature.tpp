/// \file quadrature.tpp
/// \brief numerical integration at the interface
/// \author LNM RWTH Aachen: Joerg Grande; SC RWTH Aachen:

/*
 * This file is part of DROPS.
 *
 * DROPS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DROPS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with DROPS. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright 2011 LNM/SC RWTH Aachen, Germany
 */

#include <memory>
#include "num/discretize.h"

namespace DROPS {

template <class GridFunT>
  typename ValueHelperCL<GridFunT>::value_type
  quad_impl (typename CompositeQuadratureTypesNS::const_weight_iterator w_iter, const GridFunT& f, Uint begin, Uint end)
{
    typedef typename ValueHelperCL<GridFunT>::value_type value_type;
    value_type sum= value_type();
    while (begin != end)
       sum+= (*w_iter++)*f[begin++];
    return sum;
}

template <class GridFunT>
  typename ValueHelperCL<GridFunT>::value_type
  quad (const GridFunT& f, double absdet, const QuadDomainCL& dom, TetraSignEnum s=AllTetraC)
{
    Uint begin= dom.dof_begin( s);
    Uint end=   dom.dof_end(   s);
    typename QuadDomainCL::const_weight_iterator w_iter= dom.weight_begin( s);

    return quad_impl(w_iter, f, begin, end)*absdet;
}

template <class GridFunT>
  inline void
  quad (const GridFunT& f, double absdet, const QuadDomainCL& dom,
    typename ValueHelperCL<GridFunT>::value_type& neg_int,
    typename ValueHelperCL<GridFunT>::value_type& pos_int)
{
    neg_int= quad( f, absdet, dom, NegTetraC);
    pos_int= quad( f, absdet, dom, PosTetraC);
}

///\brief Helper to quad_{neg,pos}_integrand
/// Integrate a integrand, that is defined only on either the negative or the positive tetras. It does not work for standard integrands.
template <class GridFunT>
  typename ValueHelperCL<GridFunT>::value_type
  quad_single_domain_integrand (const GridFunT& f, double absdet, const QuadDomainCL& dom, TetraSignEnum s)
{
    typename QuadDomainCL::const_weight_iterator w_iter= dom.weight_begin( s);
    const Uint end= dom.dof_end( s) - dom.dof_begin( s);

    return quad_impl(w_iter, f, 0, end)*absdet;
}

template <class GridFunT>
  inline typename ValueHelperCL<GridFunT>::value_type
  quad_neg_integrand (const GridFunT& f, double absdet, const QuadDomainCL& dom)
{
    return quad_single_domain_integrand( f, absdet, dom, NegTetraC);
}

template <class GridFunT>
  inline typename ValueHelperCL<GridFunT>::value_type
  quad_pos_integrand (const GridFunT& f, double absdet, const QuadDomainCL& dom)
{
    return quad_single_domain_integrand( f, absdet, dom, PosTetraC);
}

template <class GridFunT>
  typename ValueHelperCL<GridFunT>::value_type
  quad_2D (const GridFunT& f, const QuadDomain2DCL& dom)
{
    Uint begin= dom.dof_begin();
    Uint end=   dom.dof_end();
    typename QuadDomain2DCL::const_weight_iterator w_iter= dom.weight_begin();

    return quad_impl( w_iter, f, begin, end);
}

template <class QuadDataT>
  const QuadDomainCL&
  make_CompositeQuadDomain (QuadDomainCL& q, const TetraPartitionCL& p)
{
    const Uint num_nodes= QuadDataT::NumNodesC;

    q.vertexes_.resize( 0);
    q.vertexes_.reserve( num_nodes*p.tetra_size());
    q.pos_begin_= q.neg_end_= num_nodes*p.tetra_size( NegTetraC);
    q.weights_.resize( num_nodes*p.tetra_size());
    q.all_weights_begin_= 0;
    q.pos_weights_begin_= q.pos_begin_;

    const typename TetraPartitionCL::const_vertex_iterator partition_vertexes= p.vertex_begin();
    const typename QuadDomainCL::WeightContT tetra_weights( QuadDataT::Weight, num_nodes);
    Uint w_begin= 0;
    QRDecompCL<4,4> qr;
    SMatrixCL<4,4>& T= qr.GetMatrix();
    for (typename TetraPartitionCL::const_tetra_iterator it= p.tetra_begin(); it != p.tetra_end();
        ++it, w_begin+= num_nodes) {
        for (int i= 0; i < 4; ++i)
            T.col( i, partition_vertexes[(*it)[i]]);
        for (Uint i= 0; i < num_nodes; ++i)
            q.vertexes_.push_back( T*QuadDataT::Node[i]);
        qr.prepare_solve();
        q.weights_[std::slice( w_begin, num_nodes, 1)]= std::fabs( qr.Determinant_R())*tetra_weights;
    }
    return q;
}

inline const QuadDomainCL&
make_CompositeQuad5Domain (QuadDomainCL& q, const TetraPartitionCL& p)
{
    return make_CompositeQuadDomain<Quad5DataCL>( q, p);
}

inline const QuadDomainCL&
make_CompositeQuad3Domain (QuadDomainCL& q, const TetraPartitionCL& p)
{
    return make_CompositeQuadDomain<Quad3DataCL>( q, p);
}

template <class SubdivisionT>
  ExtrapolationToZeroCL::ExtrapolationToZeroCL (Uint num_level, const SubdivisionT& s)
    : num_intervals_( num_level), f0_( num_level)
{
    if (num_level == 0)
        throw DROPSErrCL( "ExtrapolationToZeroCL: At least one level is needed.");

    std::vector<VecT> f( num_level);
    VecT x( num_level);
    for (Uint i= 0; i < num_level; ++i) {
        num_intervals_[i]= s( i);
        x[i]= 1./num_intervals_[i];
        f[i].resize( num_level);
        f[i][i]= 1.;
    }
    compute_divided_differences( x, f);
    VecT der0( num_level);
    evaluate_newton_polynomial_and_derivative( x, f, 0., f0_, der0);
    eliminate_linear_term( x, f0_, der0);

    for (Uint i= 0; i < num_level; ++i)
        std::cerr << weights()[i] << ' ';
    std::cerr << std::endl;
}

/// \brief Multiply the weight for each level with the extrapolation factor and copy it to weights.
void
copy_weights (const std::vector<CompositeQuadratureTypesNS::WeightContT>& w_vec, const std::vector<Uint>& w_pos_begin,
    const std::valarray<double>& w_factor, CompositeQuadratureTypesNS::WeightContT& weights);

template <class QuadDataT, class LocalFET>
  const QuadDomainCL&
  make_ExtrapolatedQuadDomain (QuadDomainCL& q, const LocalFET& ls, const ExtrapolationToZeroCL& extra)
{
    q.vertexes_.resize( 0);

    typename QuadDomainCL::VertexContT pos_vertexes; // temporary container for the positive vertexes
    std::vector<QuadDomainCL::WeightContT> w_vec; // the weights for each level
    w_vec.reserve( extra.num_level());
    std::vector<Uint> w_pos_begin; // begin of the positive weights on each level
    w_pos_begin.reserve( extra.num_level());

    TetraPartitionCL partition;
    QuadDomainCL qdom;
    std::valarray<double> ls_val; // values of the level-set function in the lattice-vertexes
    // Accumulate quadrature-points and weights for each level
    for (Uint i= 0; i < extra.num_level(); ++i) {
        const Uint num_intervals= extra.num_intervals( i);
        const PrincipalLatticeCL& lat= PrincipalLatticeCL::instance( num_intervals);
        ls_val.resize( lat.num_vertexes());
        for (typename PrincipalLatticeCL::const_vertex_iterator it= lat.vertex_begin(), end= lat.vertex_end(); it != end; ++it)
            ls_val[it - lat.vertex_begin()]= ls( *it);
        partition.make_partition<SortedVertexPolicyCL, MergeCutPolicyCL>( num_intervals, ls_val);
        make_CompositeQuadDomain<QuadDataT>( qdom, partition);
        std::copy( qdom.vertex_begin( NegTetraC), qdom.vertex_end( NegTetraC), std::back_inserter( q.vertexes_));
        std::copy( qdom.vertex_begin( PosTetraC), qdom.vertex_end( PosTetraC), std::back_inserter( pos_vertexes));
        w_vec.push_back( QuadDomainCL::WeightContT( qdom.weight_begin(), qdom.size()));
        w_pos_begin.push_back( qdom.size( NegTetraC));
    }
    // Setup the data for the quadrature points
    q.pos_begin_= q.neg_end_= q.vertexes_.size();
    q.vertexes_.resize( q.vertexes_.size() + pos_vertexes.size());
    std::copy( pos_vertexes.begin(), pos_vertexes.end(), q.vertexes_.begin() + q.pos_begin_);

    // Compute the extrapolated weights
    q.pos_weights_begin_= q.pos_begin_;
    q.all_weights_begin_= 0;
    copy_weights( w_vec, w_pos_begin, extra.weights(), q.weights_);
    return q;
}

template <class LocalFET>
  const QuadDomainCL&
  make_ExtrapolatedQuad5Domain (QuadDomainCL& q, const LocalFET& ls, const ExtrapolationToZeroCL& extra)
{
    return make_ExtrapolatedQuadDomain<Quad5DataCL>( q, ls, extra);
}


template <class QuadDataT>
  const QuadDomain2DCL&
  make_CompositeQuadDomain2D (QuadDomain2DCL& q, const SurfacePatchCL& p, const TetraCL& t)
{
    const Uint num_nodes= QuadDataT::NumNodesC;

    q.vertexes_.resize( 0);
    q.vertexes_.reserve( num_nodes*p.triangle_size());
    q.weights_.resize( num_nodes*p.triangle_size());

    const typename SurfacePatchCL::const_vertex_iterator partition_vertexes= p.vertex_begin();
    const typename QuadDomainCL::WeightContT triangle_weights( QuadDataT::Weight, num_nodes);
    Uint w_begin= 0;
    BaryCoordCL tri_bary[3];
    Point3DCL   tri[3];
    BaryCoordCL* const nodes= new BaryCoordCL[QuadDataT::NumNodesC];
    for (SurfacePatchCL::const_triangle_iterator it= p.triangle_begin(); it != p.triangle_end();
        ++it, w_begin+= num_nodes) {
        for (int i= 0; i < 3; ++i) {
            tri_bary[i]= partition_vertexes[(*it)[i]];
            tri[i]= GetWorldCoord( t, tri_bary[i]);
        }
        QuadDataT::SetInterface( tri_bary, nodes);
        q.vertexes_.insert( q.vertexes_.end(), nodes, nodes + num_nodes);
        const double absdet= FuncDet2D( tri[1] - tri[0], tri[2] - tri[0]);
        q.weights_[std::slice( w_begin, num_nodes, 1)]= absdet*triangle_weights;
    }
    delete[] nodes;

    return q;
}

inline const QuadDomain2DCL&
make_CompositeQuad5Domain2D (QuadDomain2DCL& q, const SurfacePatchCL& p, const TetraCL& t)
{
    return make_CompositeQuadDomain2D<Quad5_2DDataCL>( q, p, t);
}

/// \brief Multiply the weight for each level with the extrapolation factor and copy it to weights.
void
copy_weights_surface (const std::vector<CompositeQuadratureTypesNS::WeightContT>& w_vec,
    const std::valarray<double >& w_factor, CompositeQuadratureTypesNS::WeightContT& weights);

template <class QuadDataT, class LocalFET>
  const QuadDomain2DCL&
  make_ExtrapolatedQuadDomain2D (QuadDomain2DCL& q, const LocalFET& ls, const TetraCL& t, const ExtrapolationToZeroCL& extra)
{
    q.vertexes_.resize( 0);

    std::vector<QuadDomain2DCL::WeightContT> w_vec; // the weights for each level
    w_vec.reserve( extra.num_level());

    SurfacePatchCL partition;
    QuadDomain2DCL qdom;
    std::valarray<double> ls_val; // values of the level-set function in the lattice-vertexes
    // Accumulate quadrature-points and weights for each level
    for (Uint i= 0; i < extra.num_level(); ++i) {
        const Uint num_intervals= extra.num_intervals( i);
        const PrincipalLatticeCL& lat= PrincipalLatticeCL::instance( num_intervals);
        ls_val.resize( lat.num_vertexes());
        for (typename PrincipalLatticeCL::const_vertex_iterator it= lat.vertex_begin(), end= lat.vertex_end(); it != end; ++it)
            ls_val[it - lat.vertex_begin()]= ls( *it);
        partition.make_patch<MergeCutPolicyCL>( num_intervals, ls_val);
        make_CompositeQuadDomain2D<QuadDataT>( qdom, partition, t);
        std::copy( qdom.vertex_begin(), qdom.vertex_end(), std::back_inserter( q.vertexes_));
        w_vec.push_back( QuadDomain2DCL::WeightContT( qdom.weight_begin(), qdom.size()));
    }

    // Compute the extrapolated weights
    copy_weights_surface( w_vec, extra.weights(), q.weights_);
    return q;

}

template <class LocalFET>
  inline const QuadDomain2DCL&
  make_ExtrapolatedQuad5Domain2D (QuadDomain2DCL& q, const LocalFET& ls, const TetraCL& t, const ExtrapolationToZeroCL& extra)
{
    return make_ExtrapolatedQuadDomain2D<Quad5_2DDataCL>( q, ls, t, extra);
}

} // end of namespace DROPS
