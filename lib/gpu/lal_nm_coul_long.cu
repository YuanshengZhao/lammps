// **************************************************************************
//                               nm_coul_long.cu
//                             -------------------
//                           W. Michael Brown (ORNL)
//
//  Device code for acceleration of the nm/cut/coul/long pair style
//
// __________________________________________________________________________
//    This file is part of the LAMMPS Accelerator Library (LAMMPS_AL)
// __________________________________________________________________________
//
//    begin                :
//    email                : brownw@ornl.gov
// ***************************************************************************

#if defined(NV_KERNEL) || defined(USE_HIP)

#include "lal_aux_fun1.h"
#ifndef _DOUBLE_DOUBLE
_texture( pos_tex,float4);
_texture( q_tex,float);
#else
_texture_2d( pos_tex,int4);
_texture( q_tex,int2);
#endif

#else
#define pos_tex x_
#define q_tex q_
#endif

__kernel void k_nm_coul_long(const __global numtyp4 *restrict x_,
                             const __global numtyp4 *restrict nm1,
                             const __global numtyp4 *restrict nm3,
                             const int lj_types,
                             const __global numtyp *restrict sp_lj_in,
                             const __global int *dev_nbor,
                             const __global int *dev_packed,
                             __global acctyp4 *restrict ans,
                             __global acctyp *restrict engv,
                             const int eflag, const int vflag, const int inum,
                             const int nbor_pitch,
                             const __global numtyp *restrict q_,
                             const numtyp cut_coulsq, const numtyp qqrd2e,
                             const numtyp g_ewald, const int t_per_atom) {
  int tid, ii, offset;
  atom_info(t_per_atom,ii,tid,offset);

  __local numtyp sp_lj[8];
  int n_stride;
  local_allocate_store_charge();

  sp_lj[0]=sp_lj_in[0];
  sp_lj[1]=sp_lj_in[1];
  sp_lj[2]=sp_lj_in[2];
  sp_lj[3]=sp_lj_in[3];
  sp_lj[4]=sp_lj_in[4];
  sp_lj[5]=sp_lj_in[5];
  sp_lj[6]=sp_lj_in[6];
  sp_lj[7]=sp_lj_in[7];

  acctyp4 f;
  f.x=(acctyp)0; f.y=(acctyp)0; f.z=(acctyp)0;
  acctyp energy, e_coul, virial[6];
  if (EVFLAG) {
    energy=(acctyp)0;
    e_coul=(acctyp)0;
    for (int i=0; i<6; i++) virial[i]=(acctyp)0;
  }

  if (ii<inum) {
    int nbor, nbor_end;
    int i, numj;
    nbor_info(dev_nbor,dev_packed,nbor_pitch,t_per_atom,ii,offset,i,numj,
              n_stride,nbor_end,nbor);

    numtyp4 ix; fetch4(ix,i,pos_tex); //x_[i];
    numtyp qtmp; fetch(qtmp,i,q_tex);
    int itype=ix.w;

    for ( ; nbor<nbor_end; nbor+=n_stride) {
      int j=dev_packed[nbor];

      numtyp factor_lj, factor_coul;
      factor_lj = sp_lj[sbmask(j)];
      factor_coul = (numtyp)1.0-sp_lj[sbmask(j)+4];
      j &= NEIGHMASK;

      numtyp4 jx; fetch4(jx,j,pos_tex); //x_[j];
      int jtype=jx.w;

      // Compute r12
      numtyp delx = ix.x-jx.x;
      numtyp dely = ix.y-jx.y;
      numtyp delz = ix.z-jx.z;
      numtyp rsq = delx*delx+dely*dely+delz*delz;

      int mtype=itype*lj_types+jtype;
      if (rsq<nm1[mtype].z) {
        numtyp r2inv=ucl_recip(rsq);
        numtyp forcecoul, force_lj, force, prefactor, _erfc,rninv,rminv;

        if (rsq < nm1[mtype].w) {
          rninv = ucl_powr(r2inv*nm3[mtype].w,nm3[mtype].x/(numtyp)2.0);
          rminv = ucl_powr(r2inv*nm3[mtype].w,nm3[mtype].y/(numtyp)2.0);
          force_lj = factor_lj*nm1[mtype].y *
            (rninv + nm1[mtype].x*rminv );
        } else
          force_lj = (numtyp)0.0;

        if (rsq < cut_coulsq) {
          numtyp r = ucl_rsqrt(r2inv);
          numtyp grij = g_ewald * r;
          numtyp expm2 = ucl_exp(-grij*grij);
          numtyp t = ucl_recip((numtyp)1.0 + EWALD_P*grij);
          _erfc = t * (A1+t*(A2+t*(A3+t*(A4+t*A5)))) * expm2;
          fetch(prefactor,j,q_tex);
          prefactor *= qqrd2e * qtmp/r;
          forcecoul = prefactor * (_erfc + EWALD_F*grij*expm2-factor_coul);
        } else
          forcecoul = (numtyp)0.0;

        force = (force_lj + forcecoul) * r2inv;

        f.x+=delx*force;
        f.y+=dely*force;
        f.z+=delz*force;

        if (EVFLAG && eflag) {
          if (rsq < cut_coulsq)
            e_coul += prefactor*(_erfc-factor_coul);
          if (rsq < nm1[mtype].w) {
            numtyp e=nm1[mtype].y/(nm3[mtype].y*nm3[mtype].x)*(nm3[mtype].y*rninv+ nm1[mtype].x*nm3[mtype].x*rminv);
            energy+=factor_lj*(e-nm3[mtype].z);
          }
        }
        if (EVFLAG && vflag) {
          virial[0] += delx*delx*force;
          virial[1] += dely*dely*force;
          virial[2] += delz*delz*force;
          virial[3] += delx*dely*force;
          virial[4] += delx*delz*force;
          virial[5] += dely*delz*force;
        }
      }

    } // for nbor
  } // if ii
  store_answers_q(f,energy,e_coul,virial,ii,inum,tid,t_per_atom,offset,eflag,
                  vflag,ans,engv);
}

__kernel void k_nm_coul_long_fast(const __global numtyp4 *restrict x_,
                                  const __global numtyp4 *restrict nm1_in,
                                  const __global numtyp4 *restrict nm3_in,
                                  const __global numtyp *restrict sp_lj_in,
                                  const __global int *dev_nbor,
                                  const __global int *dev_packed,
                                  __global acctyp4 *restrict ans,
                                  __global acctyp *restrict engv,
                                  const int eflag, const int vflag,
                                  const int inum,  const int nbor_pitch,
                                  const __global numtyp *restrict q_,
                                  const numtyp cut_coulsq, const numtyp qqrd2e,
                                  const numtyp g_ewald, const int t_per_atom) {
  int tid, ii, offset;
  atom_info(t_per_atom,ii,tid,offset);

  __local numtyp4 nm1[MAX_SHARED_TYPES*MAX_SHARED_TYPES];
  __local numtyp4 nm3[MAX_SHARED_TYPES*MAX_SHARED_TYPES];
  __local numtyp sp_lj[8];
  int n_stride;
  local_allocate_store_charge();

  if (tid<8)
    sp_lj[tid]=sp_lj_in[tid];
  if (tid<MAX_SHARED_TYPES*MAX_SHARED_TYPES) {
    nm1[tid]=nm1_in[tid];
    // if (EVFLAG && eflag)
      nm3[tid]=nm3_in[tid];
  }

  acctyp4 f;
  f.x=(acctyp)0; f.y=(acctyp)0; f.z=(acctyp)0;
  acctyp energy, e_coul, virial[6];
  if (EVFLAG) {
    energy=(acctyp)0;
    e_coul=(acctyp)0;
    for (int i=0; i<6; i++) virial[i]=(acctyp)0;
  }

  __syncthreads();

  if (ii<inum) {
    int nbor, nbor_end;
    int i, numj;
    nbor_info(dev_nbor,dev_packed,nbor_pitch,t_per_atom,ii,offset,i,numj,
              n_stride,nbor_end,nbor);

    numtyp4 ix; fetch4(ix,i,pos_tex); //x_[i];
    numtyp qtmp; fetch(qtmp,i,q_tex);
    int iw=ix.w;
    int itype=fast_mul((int)MAX_SHARED_TYPES,iw);

    for ( ; nbor<nbor_end; nbor+=n_stride) {
      int j=dev_packed[nbor];

      numtyp factor_lj, factor_coul;
      factor_lj = sp_lj[sbmask(j)];
      factor_coul = (numtyp)1.0-sp_lj[sbmask(j)+4];
      j &= NEIGHMASK;

      numtyp4 jx; fetch4(jx,j,pos_tex); //x_[j];
      int mtype=itype+jx.w;

      // Compute r12
      numtyp delx = ix.x-jx.x;
      numtyp dely = ix.y-jx.y;
      numtyp delz = ix.z-jx.z;
      numtyp rsq = delx*delx+dely*dely+delz*delz;

      if (rsq<nm1[mtype].z) {
        numtyp r2inv=ucl_recip(rsq);
        numtyp forcecoul, force_lj, force, prefactor, _erfc,rninv,rminv;;

        if (rsq < nm1[mtype].w) {
          rninv = ucl_powr(r2inv*nm3[mtype].w,nm3[mtype].x/(numtyp)2.0);
          rminv = ucl_powr(r2inv*nm3[mtype].w,nm3[mtype].y/(numtyp)2.0);
          force_lj = factor_lj*nm1[mtype].y *
            (rninv +nm1[mtype].x * rminv );
        } else
          force_lj = (numtyp)0.0;

        if (rsq < cut_coulsq) {
          numtyp r = ucl_rsqrt(r2inv);
          numtyp grij = g_ewald * r;
          numtyp expm2 = ucl_exp(-grij*grij);
          numtyp t = ucl_recip((numtyp)1.0 + EWALD_P*grij);
          _erfc = t * (A1+t*(A2+t*(A3+t*(A4+t*A5)))) * expm2;
          fetch(prefactor,j,q_tex);
          prefactor *= qqrd2e * qtmp/r;
          forcecoul = prefactor * (_erfc + EWALD_F*grij*expm2-factor_coul);
        } else
          forcecoul = (numtyp)0.0;

        force = (force_lj + forcecoul) * r2inv;

        f.x+=delx*force;
        f.y+=dely*force;
        f.z+=delz*force;

        if (EVFLAG && eflag) {
          if (rsq < cut_coulsq)
            e_coul += prefactor*(_erfc-factor_coul);
          if (rsq < nm1[mtype].w) {
            numtyp e=nm1[mtype].y/(nm3[mtype].y*nm3[mtype].x)*(nm3[mtype].y*rninv+nm1[mtype].x *nm3[mtype].x*rminv);
            energy+=factor_lj*(e-nm3[mtype].z);
          }
        }
        if (EVFLAG && vflag) {
          virial[0] += delx*delx*force;
          virial[1] += dely*dely*force;
          virial[2] += delz*delz*force;
          virial[3] += delx*dely*force;
          virial[4] += delx*delz*force;
          virial[5] += dely*delz*force;
        }
      }

    } // for nbor
  } // if ii
  store_answers_q(f,energy,e_coul,virial,ii,inum,tid,t_per_atom,offset,eflag,
                  vflag,ans,engv);
}

