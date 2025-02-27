/***************************************************************************
                               nm_coul_long.h
                             -------------------
                            W. Michael Brown (ORNL)

  Class for acceleration of the nm/cut/coul/long pair style.

 __________________________________________________________________________
    This file is part of the LAMMPS Accelerator Library (LAMMPS_AL)
 __________________________________________________________________________

    begin                :
    email                : brownw@ornl.gov
 ***************************************************************************/

#ifndef LAL_NM_COUL_LONG_H
#define LAL_NM_COUL_LONG_H

#include "lal_base_charge.h"

namespace LAMMPS_AL {

template <class numtyp, class acctyp>
class NMCoulLong : public BaseCharge<numtyp, acctyp> {
 public:
  NMCoulLong();
  ~NMCoulLong();

  /// Clear any previous data and set up for a new LAMMPS run
  /** \param max_nbors initial number of rows in the neighbor matrix
    * \param cell_size cutoff + skin
    * \param gpu_split fraction of particles handled by device
    *
    * Returns:
    * -  0 if successful
    * - -1 if fix gpu not found
    * - -3 if there is an out of memory error
    * - -4 if the GPU library was not compiled for GPU
    * - -5 Double precision is not supported on card **/
  int init(const int ntypes, double **host_cutsq, double **host_gma,
            double **host_fnm, double **host_nn, double **host_mm,
            double **host_r02, double **host_offset, double *host_special_lj,
           const int nlocal, const int nall, const int max_nbors,
           const int maxspecial, const double cell_size,
           const double gpu_split, FILE *screen, double **host_cut_ljsq,
           const double host_cut_coulsq, double *host_special_coul,
           const double qqrd2e, const double g_ewald);

  /// Send updated coeffs from host to device (to be compatible with fix adapt)
  void reinit(const int ntypes, double **host_cutsq, double **host_gma,
                  double **host_fnm, double **host_nn, double **host_mm,
                  double **host_r02, double **host_offset, double **host_cut_ljsq);

  /// Clear all host and device data
  /** \note This is called at the beginning of the init() routine **/
  void clear();

  /// Returns memory usage on device per atom
  int bytes_per_atom(const int max_nbors) const;

  /// Total host memory used by library for pair style
  double host_memory_usage() const;

  // --------------------------- TYPE DATA --------------------------

  /// lj1.x = gma, lj1.y = fnm, lj1.z = cutsq, lj1.w = cutsq_vdw
  UCL_D_Vec<numtyp4> nm1;
  /// lj3.x = nn, lj3.y = mm, lj3.z = offset, .w = r02
  UCL_D_Vec<numtyp4> nm3;
  /// Special LJ values [0-3] and Special Coul values [4-7]
  UCL_D_Vec<numtyp> sp_lj;

  /// If atom type constants fit in shared memory, use fast kernels
  bool shared_types;

  /// Number of atom types
  int _lj_types;

  numtyp _cut_coulsq, _qqrd2e, _g_ewald;

 private:
  bool _allocated;
  int loop(const int eflag, const int vflag);
};

}

#endif
