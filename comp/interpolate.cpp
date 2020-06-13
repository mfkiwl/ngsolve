/**********************************************************************/
/* File:   interpolate.cpp                                            */
/* Author: L Kogler, M Neunteufel, J Schoeberl                        */
/* Date:   June 2020                                                  */
/**********************************************************************/

/* 
   Interpolation of CoefficientFunctions using
   dual shapes
*/

#include <comp.hpp>
#include <variant>
#include "../fem/integratorcf.hpp"


namespace ngcomp
{
  
  class InterpolationCoefficientFunction : public T_CoefficientFunction<InterpolationCoefficientFunction>
  {
  protected:
    shared_ptr<CoefficientFunction> func;
    shared_ptr<FESpace> fes;
    int bonus_intorder;

    Array<shared_ptr<BilinearFormIntegrator>> bli;
    Array<shared_ptr<BilinearFormIntegrator>> single_bli;
    // shared_ptr<CoefficientFunction> dual_diffop;
    shared_ptr<DifferentialOperator> dual_diffop;

    VorB vb;
    
  public:
    InterpolationCoefficientFunction (shared_ptr<CoefficientFunction> f, shared_ptr<FESpace> afes,
                                      int abonus_intorder)
      : T_CoefficientFunction<InterpolationCoefficientFunction>(f->Dimension(), f->IsComplex()),
      func(f), fes(afes), bonus_intorder(abonus_intorder)
    {
      this->SetDimensions (func->Dimensions());
      this->elementwise_constant = func->ElementwiseConstant();

      // copied from Set (dualshapes)
      
      vb = VOL;    // for the moment only 

      /** Trial-Proxy **/
      auto single_evaluator =  fes->GetEvaluator(vb);
      if (dynamic_pointer_cast<BlockDifferentialOperator>(single_evaluator))
        single_evaluator = dynamic_pointer_cast<BlockDifferentialOperator>(single_evaluator)->BaseDiffOp();
      auto trial = make_shared<ProxyFunction>(fes, false, false, single_evaluator,
                                              nullptr, nullptr, nullptr, nullptr, nullptr);

      /** Test-Proxy (dual) **/
      auto dual_evaluator = fes->GetAdditionalEvaluators()["dual"];
      for (VorB avb = VOL; avb < vb; avb++) {
        dual_evaluator = dual_evaluator->GetTrace();
        if ( dual_evaluator == nullptr )
          { throw Exception(fes->GetClassName() + string(" has no dual trace operator for vb = ") + \
                            to_string(avb) + string(" -> ") + to_string(avb + 1) + string("!")); }
      }
      if (dynamic_pointer_cast<BlockDifferentialOperator>(dual_evaluator))
        dual_evaluator = dynamic_pointer_cast<BlockDifferentialOperator>(dual_evaluator)->BaseDiffOp();
      auto dual = make_shared<ProxyFunction>(fes, true, false, dual_evaluator,
                                             nullptr, nullptr, nullptr, nullptr, nullptr);

      dual_diffop = dual_evaluator;

      for (auto element_vb : fes->GetDualShapeNodes(vb))
        {
          shared_ptr<CoefficientFunction> dual_trial;
          if (dual -> Dimension() == 1)
            { dual_trial = dual * trial; }
          else
            { dual_trial = InnerProduct(dual, trial); }
          auto bfi = make_shared<SymbolicBilinearFormIntegrator> (dual_trial, vb, element_vb);
	  bfi->SetSimdEvaluate(false);  // dual are not simded, yet
	  bli.Append(bfi);
	  if (auto block_bfi = dynamic_pointer_cast<BlockBilinearFormIntegrator> (bfi)) {
	    auto sbfi = block_bfi->BlockPtr();
	    sbfi->SetSimdEvaluate(false);
	    single_bli.Append(sbfi);
	  }
	  else
	    { single_bli.Append(bfi); }
	}
    }


    template <typename MIR, typename T, ORDERING ORD>
    void T_Evaluate_impl (const MIR & ir, BareSliceMatrix<T,ORD> values) const
    {
      // #ifdef FIRSTDRAFT
      LocalHeapMem<100000> lh("interpolate");

      // static Timer t("interpolate");
      // RegionTracer reg(TaskManager::GetThreadId(), t);    

      const ElementTransformation & trafo = ir.GetTransformation();
      // const MeshAccess & ma = *static_cast<const MeshAccess*> (trafo.GetMesh());
      ElementId ei = trafo.GetElementId();
      auto & fel = fes->GetFE(ei, lh);
      // int dim   = fes->GetDimension();
      int dim = Dimension();

      
      // cout << " eval for ei " << ei << endl;
      // cout << " ndof = " << fel.GetNDof() << endl;

      // if (dim != 1)
      // { throw Exception("Dim != 1 porbably does not work (yet)"); }

      /** func * dual_shape **/
      FlatVector<T> elflux(fel.GetNDof(), lh);

      FlatVector<T> elfluxadd(fel.GetNDof(), lh);  elflux = 0; // non-SIMD version
      for (auto el_vb : fes->GetDualShapeNodes(trafo.VB()))
	{
          if (el_vb == VOL)
            {
              IntegrationRule ir(fel.ElementType(), 2*fel.Order()+bonus_intorder);
              auto & mir = trafo(ir, lh);
              FlatMatrix<T> mflux(ir.Size(), dim, lh);
              func->Evaluate (mir, mflux);
              for (size_t j : Range(mir))
                mflux.Row(j) *= mir[j].GetWeight();
              dual_diffop -> ApplyTrans (fel, mir, mflux, elfluxadd, lh);
              elflux += elfluxadd;
            }
          else
            {
              Facet2ElementTrafo f2el (fel.ElementType(), el_vb);
              for (int locfnr : Range(f2el.GetNFacets()))
                {
                  // SIMD does not work yet
                  // SIMD_IntegrationRule irfacet(f2el.FacetType(locfnr), 2 * fel.Order());
                  IntegrationRule irfacet(f2el.FacetType(locfnr), 2*fel.Order()+bonus_intorder);
                  auto & irvol = f2el(locfnr, irfacet, lh);
                  auto & mir = trafo(irvol, lh);
                  mir.ComputeNormalsAndMeasure(fel.ElementType(), locfnr);
                  
                  // FlatMatrix<T,ORD> mflux(dim, irfacet.Size(), lh);
                  // func->Evaluate (mir, mflux);
                  // for (size_t j : Range(mir))
                  // 	mflux.Col(j) *= mir[j].GetWeight();
                  // SIMD only
                  // dual_diffop->AddTrans (fel, mir, mflux, elflux);
                  // NON-simd version
                  
                  FlatMatrix<T> mflux(irfacet.Size(), dim, lh);
                  func->Evaluate (mir, mflux);
                  for (size_t j : Range(mir))
                    mflux.Row(j) *= mir[j].GetWeight();
                  // cout << "mflux = " << mflux << endl;
                  dual_diffop -> ApplyTrans (fel, mir, mflux, elfluxadd, lh);
                  // cout << " elfluxadd = " << endl << elfluxadd << endl;
                  elflux += elfluxadd;
                }
            }
        }
      
      /** Calc Element Matrix - shape * dual_shape **/
      FlatMatrix<double> elmat(fel.GetNDof(), lh);
      elmat = 0.0;
      bool symmetric_so_far = false;

      auto & nonconst_trafo = const_cast<ElementTransformation&>(trafo);
      auto saveud = nonconst_trafo.userdata;
      for (auto sbfi : single_bli)
        sbfi->CalcElementMatrixAdd (fel, trafo, elmat, symmetric_so_far, lh);
      nonconst_trafo.userdata = saveud;

      /** Invert Element Matrix and Solve for RHS **/
      CalcInverse(elmat); // Not Symmetric !
      

      // cout << "interpolation elmat = " << endl << elmat << endl;
      
      /** Calc coefficients of Interpolation **/
      FlatVector<double> coeffs(fel.GetNDof(), lh);
      coeffs = elmat * elflux;

      // cout << " coeffs: " << endl << coeffs << endl;

      // func->Evaluate(ir, values);
      // cout << " un-interp values: " << endl << values.AddSize(Dimension(), ir.Size()) << endl;
        
      if constexpr (ORD==ColMajor) 
        fes->GetEvaluator(vb)->Apply(fel, ir, coeffs, Trans(values), lh);
      else 
	fes->GetEvaluator(vb)->Apply(fel, ir, coeffs, values, lh);

      // cout << " values: " << endl << values.AddSize(Dimension(), ir.Size()) << endl;
    }


    template <typename MIR, typename T, ORDERING ORD>
    void T_Evaluate (const MIR & ir, BareSliceMatrix<T,ORD> values) const
    {
      if constexpr (is_same<MIR, SIMD_BaseMappedIntegrationRule>::value)
                     throw ExceptionNOSIMD ("no simd in InterpolateCF");
                    
      if constexpr(std::is_same<T, double>::value) {
	  // if constexpr(ORD == RowMajor) {
	      T_Evaluate_impl (ir, values);
	    // }
	  // else
	    // { throw Exception("Col-major does not compile (yet)"); }
	}
      else
	{ throw Exception("InterpolateCF::T_Evaluate only for double!"); }
      // func->Evaluate(ir, values);
    }


    template <typename MIR, typename T, ORDERING ORD>
    void T_Evaluate (const MIR & ir,
		     FlatArray<BareSliceMatrix<T,ORD>> input,
		     BareSliceMatrix<T,ORD> values) const
    {
      T_Evaluate (ir, values);
    }

    Array<shared_ptr<CoefficientFunction>> InputCoefficientFunctions() const override
    { return Array<shared_ptr<CoefficientFunction>>({ func }); }

    void PrintReport (ostream & ost) const override
    {
      ost << "InterpolationCF(";
      func->PrintReport(ost);
      ost << ")";
    }

    string GetDescription() const override
    {
      return "InterpolationCF";
    }

    void NonZeroPattern (const class ProxyUserData & ud,
			 FlatVector<AutoDiffDiff<1,bool>> nonzero) const override
    {
      func->NonZeroPattern(ud, nonzero);
    }

    void NonZeroPattern (const class ProxyUserData & ud,
			 FlatArray<FlatVector<AutoDiffDiff<1,bool>>> input,
			 FlatVector<AutoDiffDiff<1,bool>> values) const override
    {
      func->NonZeroPattern(ud, input, values);
    }

    void TraverseTree (const function<void(CoefficientFunction&)> & func_) override
    {
      func->TraverseTree(func_);
      func_(*this);
    }

    shared_ptr<CoefficientFunction> Diff (const CoefficientFunction * var, shared_ptr<CoefficientFunction> dir) const override
    {
      if (this == var) return dir;
      return InterpolateCF(func->Diff(var, dir),fes);
    }
  };
  
  
  shared_ptr<CoefficientFunction> InterpolateCF (shared_ptr<CoefficientFunction> func, shared_ptr<FESpace> space,
                                                 int bonus_intorder)
  {
    func->PrintReport(cout);

    if (func->GetDescription() == "ZeroCF")
      return func;

    return make_shared<InterpolationCoefficientFunction> (func, space, bonus_intorder);
  }

}