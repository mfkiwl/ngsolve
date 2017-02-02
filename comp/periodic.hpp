#ifndef FILE_PERIODIC_
#define FILE_PERIODIC_

/*********************************************************************/
/* File:   periodic.hpp                                              */
/* Author: Christopher Lackner                                       */
/* Date:   Feb. 2017                                                 */
/*********************************************************************/

namespace ngcomp
{

 // A periodic wrapper class for fespaces 

  class PeriodicFESpace : public FESpace
  {
    Array<int> dofmap; // mapping of dofs
    shared_ptr<FESpace> space;
    
  public:
    PeriodicFESpace (shared_ptr<FESpace> space, const Flags & flags);
    
    virtual ~PeriodicFESpace () { ; }
    virtual void Update (LocalHeap & lh) override;
    
    virtual void FinalizeUpdate (LocalHeap & lh) override {
      space->FinalizeUpdate(lh);
      FESpace::FinalizeUpdate(lh);
    }

    virtual string GetClassName() const { return "Periodic" + space->GetClassName(); }
    
    virtual FiniteElement & GetFE (ElementId ei, Allocator & alloc) const override;

    virtual size_t GetNDof () const { return space->GetNDof(); }
    virtual size_t GetNDofLevel (int level) const { return space->GetNDofLevel(level); }

    virtual void GetDofNrs(ElementId ei, Array<int> & dnums) const override;

    virtual void GetVertexDofNrs (int vnr,  Array<DofId> & dnums) const override
    { space->GetVertexDofNrs(vnr, dnums); }
    virtual void GetEdgeDofNrs (int ednr, Array<DofId> & dnums) const override
    { space->GetEdgeDofNrs (ednr, dnums); }
    virtual void GetFaceDofNrs (int fanr, Array<DofId> & dnums) const override
    { space->GetFaceDofNrs(fanr, dnums); }
    virtual void GetInnerDofNrs (int elnr, Array<DofId> & dnums) const override
    { space->GetInnerDofNrs(elnr, dnums); }

    virtual void VTransformMR (ElementId ei,
			       SliceMatrix<double> mat, TRANSFORM_TYPE tt) const override
    { space-> VTransformMR(ei, mat, tt); }
    virtual void VTransformMC (ElementId ei,
                               SliceMatrix<Complex> mat, TRANSFORM_TYPE tt) const override
    { space->VTransformMC (ei, mat, tt); }
    virtual void VTransformVR (ElementId ei,
                               SliceVector<double> vec, TRANSFORM_TYPE tt) const override
    { space->VTransformVR(ei, vec, tt); }
    virtual void VTransformVC (ElementId ei, 
                               SliceVector<Complex> vec, TRANSFORM_TYPE tt) const override
    { space->VTransformVC(ei, vec, tt); }    
    
  private:
    void GetPeriodicNodeIds(Array<std::tuple<NodeId,NodeId>> & node_ids,int idnr) const;

  };
}


#endif // FILE_PERIODIC_
