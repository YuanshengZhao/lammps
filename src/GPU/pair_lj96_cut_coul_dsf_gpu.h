/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef PAIR_CLASS
// clang-format off
PairStyle(lj96/cut/coul/dsf/gpu,PairLJ96CutCoulDSFGPU);
// clang-format on
#else

#ifndef LMP_PAIR_LJ96_CUT_COUL_DSF_GPU_H
#define LMP_PAIR_LJ96_CUT_COUL_DSF_GPU_H

#include "pair_lj96_cut_coul_dsf.h"

namespace LAMMPS_NS {

class PairLJ96CutCoulDSFGPU : public PairLJ96CutCoulDSF {
 public:
  PairLJ96CutCoulDSFGPU(LAMMPS *lmp);
  ~PairLJ96CutCoulDSFGPU();
  void cpu_compute(int, int, int, int, int *, int *, int **);
  void compute(int, int);
  void init_style();
  double memory_usage();

  enum { GPU_FORCE, GPU_NEIGH, GPU_HYB_NEIGH };

 private:
  int gpu_mode;
  double cpu_time;
};

}    // namespace LAMMPS_NS
#endif
#endif

/* ERROR/WARNING messages:

E: Insufficient memory on accelerator

There is insufficient memory on one of the devices specified for the gpu
package

E: Pair style lj/cut/coul/dsf/gpu requires atom attribute q

The atom style defined does not have this attribute.

E: Cannot use newton pair with lj/cut/coul/dsf/gpu pair style

Self-explanatory.

*/
