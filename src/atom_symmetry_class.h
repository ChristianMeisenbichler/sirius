#ifndef __ATOM_SYMMETRY_CLASS_H__
#define __ATOM_SYMMETRY_CLASS_H__

/*! \page page1 Notes
    
    AtomSymmetryClass holds information about radial functions.
*/

/*! \page page2 Standard variable names

     idxlo - index of local orbital \n
     idxrf - index of radial function \n
     ia - index of atom \n
     ic - index of atom class \n
     iat - index of atom type \n
     ir - index of r-point \n
     ig - index of G-vector \n
*/

namespace sirius {

class AtomSymmetryClass
{
    private:
        
        /// symmetry class id in the range [0, N_class - 1]
        int id_;

        /// list of atoms of this class
        std::vector<int> atom_id_;
        
        /// atom type
        AtomType* atom_type_;

        /// spherical part of effective potential 
        std::vector<double> spherical_potential_;

        /// list of radial functions
        mdarray<double,3> radial_functions_;
        
        /// surface derivatives of aw radial functions
        mdarray<double,2> aw_surface_derivatives_;

        /// spherical part of radial integral
        mdarray<double,2> h_spherical_integrals_;

        /// overlap integrals
        mdarray<double,3> o_radial_integrals_;

        /// spin-orbit interaction integrals
        mdarray<double,3> so_radial_integrals_;

        /// core charge density
        std::vector<double> core_charge_density_;

        /// core eigen-value sum
        double core_eval_sum_;

        /// core leakage
        double core_leakage_;
        
        void generate_aw_radial_functions()
        {
            int nmtp = atom_type_->num_mt_points();
            
            Spline<double> s(nmtp, atom_type_->radial_grid());
            RadialSolver solver(false, -1.0 * atom_type_->zn(), atom_type_->radial_grid());
            
            std::vector<double> p;
            std::vector<double> hp;

            for (int l = 0; l < atom_type_->num_aw_descriptors(); l++)
            {
                for (int order = 0; order < (int)atom_type_->aw_descriptor(l).size(); order++)
                {
                    radial_solution_descriptor& rsd = atom_type_->aw_descriptor(l)[order];

                    int idxrf = atom_type_->indexr().index_by_l_order(l, order);

                    // find linearization energies
                    if (rsd.auto_enu)
                        solver.bound_state(rsd.n, rsd.l, spherical_potential_, rsd.enu, p);

                    solver.solve_in_mt(rsd.l, rsd.enu, rsd.dme, spherical_potential_, p, hp);

                    // normalize
                    for (int ir = 0; ir < nmtp; ir++) 
                        s[ir] = pow(p[ir], 2);
                    s.interpolate();
                    double norm = s.integrate(0);
                    norm = 1.0 / sqrt(norm);

                    for (int ir = 0; ir < nmtp; ir++)
                    {
                        radial_functions_(ir, idxrf, 0) = p[ir] * norm;
                        radial_functions_(ir, idxrf, 1) = hp[ir] * norm;
                    }
                    
                    // orthogonalize
                    for (int order1 = 0; order1 < order - 1; order1++)
                    {
                        int idxrf1 = atom_type_->indexr().index_by_l_order(l, order1);

                        for (int ir = 0; ir < nmtp; ir++)
                            s[ir] = radial_functions_(ir, idxrf, 0) * radial_functions_(ir, idxrf1, 0);
                        s.interpolate();
                        double t1 = s.integrate(0);
                        
                        for (int ir = 0; ir < nmtp; ir++)
                        {
                            radial_functions_(ir, idxrf, 0) -= radial_functions_(ir, idxrf1, 0) * t1;
                            radial_functions_(ir, idxrf, 1) -= radial_functions_(ir, idxrf1, 1) * t1;
                        }
                    }
                        
                    // normalize again
                    for (int ir = 0; ir < nmtp; ir++)
                        s[ir] = pow(radial_functions_(ir, idxrf, 0), 2);
                    s.interpolate();
                    norm = s.integrate(0);

                    if (fabs(norm) < 1e-12)
                        error(__FILE__, __LINE__, "aw radial functions are linearly dependent");

                    norm = 1.0 / sqrt(norm);

                    for (int ir = 0; ir < nmtp; ir++)
                    {
                        radial_functions_(ir, idxrf, 0) *= norm;
                        radial_functions_(ir, idxrf, 1) *= norm;
                    }

                    // radial derivative
                    double rderiv = (radial_functions_(nmtp - 1, idxrf, 0) - radial_functions_(nmtp - 2, idxrf, 0)) / 
                                    atom_type_->radial_grid().dr(nmtp - 2);
                    double R = atom_type_->mt_radius();

                    aw_surface_derivatives_(order, l) = (rderiv - radial_functions_(nmtp - 1, idxrf, 0) / R) / R;
                    
                    // divide by r
                    for (int ir = 0; ir < nmtp; ir++)
                    {
                        radial_functions_(ir, idxrf, 0) /= atom_type_->radial_grid(ir);
                        radial_functions_(ir, idxrf, 1) /= atom_type_->radial_grid(ir);
                    }
                }
            }
        }

        void generate_lo_radial_functions()
        {
            int nmtp = atom_type_->num_mt_points();
            Spline<double> s(nmtp, atom_type_->radial_grid());
            RadialSolver solver(false, -1.0 * atom_type_->zn(), atom_type_->radial_grid());
            
            double a[4][4];

            for (int idxlo = 0; idxlo < atom_type_->num_lo_descriptors(); idxlo++)
            {
                assert(atom_type_->lo_descriptor(idxlo).size() < 4);

                int idxrf = atom_type_->indexr().index_by_idxlo(idxlo);

                std::vector< std::vector<double> > p(atom_type_->lo_descriptor(idxlo).size());
                std::vector< std::vector<double> > hp(atom_type_->lo_descriptor(idxlo).size());

                for (int order = 0; order < (int)atom_type_->lo_descriptor(idxlo).size(); order++)
                {
                    radial_solution_descriptor& rsd = atom_type_->lo_descriptor(idxlo)[order];
                    
                    // find linearization energies
                    if (rsd.auto_enu)
                        solver.bound_state(rsd.n, rsd.l, spherical_potential_, rsd.enu, p[order]);

                    solver.solve_in_mt(rsd.l, rsd.enu, rsd.dme, spherical_potential_, p[order], hp[order]); 
                    
                    // normalize radial solutions and divide by r
                    for (int ir = 0; ir < nmtp; ir++)
                        s[ir] = pow(p[order][ir], 2);
                    s.interpolate();
                    double norm = s.integrate(0);
                    norm = 1.0 / sqrt(norm);

                    for (int ir = 0; ir < nmtp; ir++)
                    {
                        p[order][ir] *= (norm / atom_type_->radial_grid(ir));
                        hp[order][ir] *= (norm / atom_type_->radial_grid(ir));
                        s[ir] = p[order][ir];
                    }

                    // compute radial derivatives
                    s.interpolate();

                    for (int dm = 0; dm < (int)atom_type_->lo_descriptor(idxlo).size(); dm++)
                        a[order][dm] = s.deriv(dm, nmtp - 1);
                }

                double b[] = {0.0, 0.0, 0.0, 0.0};
                b[atom_type_->lo_descriptor(idxlo).size() - 1] = 1.0;

                int info = gesv<double>((int)atom_type_->lo_descriptor(idxlo).size(), 1, &a[0][0], 4, b, 4);

                if (info) 
                {
                    std::stringstream s;
                    s << "gesv returned " << info;
                    error(__FILE__, __LINE__, s);
                }
                
                // take linear combination of radial solutions
                for (int order = 0; order < (int)atom_type_->lo_descriptor(idxlo).size(); order++)
                    for (int ir = 0; ir < nmtp; ir++)
                    {
                        radial_functions_(ir, idxrf, 0) += b[order] * p[order][ir];
                        radial_functions_(ir, idxrf, 1) += b[order] * hp[order][ir];
                    }

                // normalize
                for (int ir = 0; ir < nmtp; ir++)
                    s[ir] = pow(radial_functions_(ir, idxrf, 0), 2);
                s.interpolate();
                double norm = s.integrate(2);
                norm = 1.0 / sqrt(norm);

                for (int ir = 0; ir < nmtp; ir++)
                {
                    radial_functions_(ir, idxrf, 0) *= norm;
                    radial_functions_(ir, idxrf, 1) *= norm;
                }
#if 0
                std::ofstream out("lo.dat");
                for (int ir = 0; ir < nmtp; ir++)
                   out << atom_type_->radial_grid()[ir] << " " << lo_radial_functions_(ir, i, 0) << std::endl;
                out.close();
#endif
                if (fabs(radial_functions_(nmtp - 1, idxrf, 0)) > 1e-10)
                {
                    std::stringstream s;
                    s << "local orbital is not zero at MT boundary" << std::endl 
                      << "  value : " << radial_functions_(nmtp - 1, idxrf, 0);
                    error(__FILE__, __LINE__, s);
                }
            }
        }

    public:
    
        AtomSymmetryClass(int id_, 
                          AtomType* atom_type_) : id_(id_),
                                                  atom_type_(atom_type_)
        {
        
        }

        void init()
        {
            aw_surface_derivatives_.set_dimensions(atom_type_->max_aw_order(), atom_type_->num_aw_descriptors());
            aw_surface_derivatives_.allocate();

            radial_functions_.set_dimensions(atom_type_->num_mt_points(), atom_type_->indexr().size(), 2);
            radial_functions_.allocate();

            h_spherical_integrals_.set_dimensions(atom_type_->indexr().size(), atom_type_->indexr().size());
            h_spherical_integrals_.allocate();
            
            o_radial_integrals_.set_dimensions(atom_type_->num_aw_descriptors(), atom_type_->indexr().max_num_rf(), 
                                               atom_type_->indexr().max_num_rf());
            o_radial_integrals_.allocate();
            
            so_radial_integrals_.set_dimensions(atom_type_->num_aw_descriptors(), atom_type_->indexr().max_num_rf(), 
                                                atom_type_->indexr().max_num_rf());
            so_radial_integrals_.allocate();
         }

        inline int id()
        {
            return id_;
        }

        inline void add_atom_id(int _atom_id)
        {
            atom_id_.push_back(_atom_id);
        }
        
        inline int num_atoms()
        {
            return (int)atom_id_.size();
        }

        inline int atom_id(int idx)
        {
            return atom_id_[idx];
        }

        void set_spherical_potential(std::vector<double>& veff)
        {
            int nmtp = atom_type_->num_mt_points();
            assert((int)veff.size() == nmtp);

            spherical_potential_.resize(atom_type_->radial_grid().size());
            
            // take current effective potential inside MT
            for (int ir = 0; ir < nmtp; ir++)
                spherical_potential_[ir] = veff[ir];

            // take potential of the free atom outside MT
            for (int ir = nmtp; ir < atom_type_->radial_grid().size(); ir++)
                spherical_potential_[ir] = atom_type_->free_atom_potential(ir) - 
                                           (atom_type_->free_atom_potential(nmtp - 1) - veff[nmtp - 1]);
#if 0
            int nmtp = atom_type_->num_mt_points();

            std::ofstream out("veff_for_enu.dat");

            for (int ir = 0; ir < nmtp; ir++)
               out << atom_type_->radial_grid()[ir] << " " << spherical_potential_[ir] << std::endl;
            out << std::endl;

            for (int ir = 0; ir < atom_type_->radial_grid().size(); ir++)
                out << atom_type_->radial_grid()[ir] << " " << atom_type_->free_atom_potential(ir) << std::endl;
            out << std::endl;
            
            for (int ir = 0; ir < atom_type_->radial_grid().size(); ir++)
                out << atom_type_->radial_grid()[ir] << " " << veff[ir] << std::endl;
            out << std::endl;

            out.close();
#endif
        }

        void generate_radial_functions()
        {
            Timer t("sirius::AtomSymmetryClass::generate_radial_functions");

            radial_functions_.zero();

            generate_aw_radial_functions();
            generate_lo_radial_functions();

            //print_enu();
        }

        /// Generate radial overlap and SO integrals

        /** In the case of spin-orbit interaction the following integrals are computed:
            \f[
                \int f_{p}(r) \Big( \frac{1}{(2 M c)^2} \frac{1}{r} \frac{d V}{d r} \Big) f_{p'}(r) r^2 dr
            \f]
            
            Relativistic mass M is defined as
            \f[
                M = 1 - \frac{1}{2 c^2} V
            \f]
        */
        void generate_radial_integrals()
        {
            Timer t("sirius::AtomSymmetryClass::generate_radial_integrals");

            int nmtp = atom_type_->num_mt_points();
            Spline<double> s(nmtp, atom_type_->radial_grid()); 

            h_spherical_integrals_.zero();
            for (int i1 = 0; i1 < atom_type_->indexr().size(); i1++)
                for (int i2 = 0; i2 < atom_type_->indexr().size(); i2++)
                {
                    // for spherical part of potential integrals are diagonal in l
                    if (atom_type_->indexr(i1).l == atom_type_->indexr(i2).l)
                    {
                        for (int ir = 0; ir < nmtp; ir++)
                            s[ir] = radial_functions_(ir, i1, 0) * radial_functions_(ir, i2, 1);
                        s.interpolate();
                        h_spherical_integrals_(i1, i2) = s.integrate(2) / y00;
                    }
                }
            
            o_radial_integrals_.zero();
            for (int l = 0; l < atom_type_->num_aw_descriptors(); l++)
            {
                int nrf = atom_type_->indexr().num_rf(l);

                for (int order1 = 0; order1 < nrf; order1++)
                {
                    int idxrf1 = atom_type_->indexr().index_by_l_order(l, order1);
                    for (int order2 = 0; order2 < nrf; order2++)
                    {
                        int idxrf2 = atom_type_->indexr().index_by_l_order(l, order2);

                        if (order1 == order2) o_radial_integrals_(l, order1, order2) = 1.0;
                        else
                        {
                            for (int ir = 0; ir < nmtp; ir++)
                                s[ir] = radial_functions_(ir, idxrf1, 0) * radial_functions_(ir, idxrf2, 0);
                            s.interpolate();
                            o_radial_integrals_(l, order1, order2) = s.integrate(2);
                        }
                    }
                }
            }

            if (true) // TODO: if it's slow, compute only when spin-orbit is turned on
            {
                double soc = pow(2 * speed_of_light, -2);

                Spline<double> s1(nmtp, atom_type_->radial_grid()); 
                
                Spline<double> ve(nmtp, atom_type_->radial_grid()); 
                for (int i = 0; i < nmtp; i++)
                    ve[i] = spherical_potential_[i] + atom_type_->zn() / atom_type_->radial_grid(i);
                ve.interpolate();

                so_radial_integrals_.zero();
                for (int l = 0; l < atom_type_->num_aw_descriptors(); l++)
                {
                    int nrf = atom_type_->indexr().num_rf(l);
    
                    for (int order1 = 0; order1 < nrf; order1++)
                    {
                        int idxrf1 = atom_type_->indexr().index_by_l_order(l, order1);
                        for (int order2 = 0; order2 < nrf; order2++)
                        {
                            int idxrf2 = atom_type_->indexr().index_by_l_order(l, order2);

                            for (int ir = 0; ir < nmtp; ir++)
                            {
                                double M = 1.0 - 2 * soc * spherical_potential_[ir];
                                // first part <f| dVe / dr |f'>
                                s[ir] = radial_functions_(ir, idxrf1, 0) * radial_functions_(ir, idxrf2, 0) * 
                                        soc * ve.deriv(1, ir) / pow(M, 2);

                                // second part <f| d(z/r) / dr |f'>
                                s1[ir] = radial_functions_(ir, idxrf1, 0) * radial_functions_(ir, idxrf2, 0) *
                                         soc * atom_type_->zn() / pow(M, 2);
                            }
                            s.interpolate();
                            s1.interpolate();

                            so_radial_integrals_(l, order1, order2) = s.integrate(1) + s1.integrate(-1);
                        }
                    }
                }
            }
        }

        inline double radial_function(int ir, int idx)
        {
            return radial_functions_(ir, idx, 0);
        }

        inline double h_spherical_integral(int i1, int i2)
        {
            return h_spherical_integrals_(i1, i2);
        }

        inline double o_radial_integral(int l, int order1, int order2)
        {
            return o_radial_integrals_(l, order1, order2);
        }
        
        inline double so_radial_integral(int l, int order1, int order2)
        {
            return so_radial_integrals_(l, order1, order2);
        }


        void print_enu()
        {
            printf("augmented waves\n");
            for (int l = 0; l < atom_type_->num_aw_descriptors(); l++)
                for (int order = 0; order < (int)atom_type_->aw_descriptor(l).size(); order++)
                {
                    radial_solution_descriptor& rsd = atom_type_->aw_descriptor(l)[order];
                    printf("n = %i   l = %i   order = %i   enu = %f\n", rsd.n, rsd.l, order, rsd.enu);
                }

            printf("local orbitals\n");
            for (int idxlo = 0; idxlo < atom_type_->num_lo_descriptors(); idxlo++)
                for (int order = 0; order < (int)atom_type_->lo_descriptor(idxlo).size(); order++)
                {
                    radial_solution_descriptor& rsd = atom_type_->lo_descriptor(idxlo)[order];
                    printf("n = %i   l = %i   order = %i   enu = %f\n", rsd.n, rsd.l, order, rsd.enu);
                }
        }

        /*!
            \brief Compute m-th order radial derivative at the MT surface
        */
        double aw_surface_dm(int l, int order, int dm)
        {
            assert(dm <= 1);

            if (dm == 0)
            {
                int idxrf = atom_type_->indexr().index_by_l_order(l, order);
                return radial_functions_(atom_type_->num_mt_points() - 1, idxrf, 0);
            } 
            else if (dm == 1)
            {
                return aw_surface_derivatives_(order, l);
            }
            else return 0.0;
        }

        void generate_core_charge_density()
        {
            Timer t("sirius::AtomSymmetryClass::generate_core_charge_density");

            if (atom_type_->num_core_electrons() == 0)
            {
                core_charge_density_.resize(atom_type_->radial_grid().size());
                memset(&core_charge_density_[0], 0, atom_type_->radial_grid().size() * sizeof(double));
                core_leakage_ = 0.0;
                return;
            }
            
            RadialSolver solver(true, -1.0 * atom_type_->zn(), atom_type_->radial_grid());
            
            Spline<double> rho(atom_type_->radial_grid().size(), atom_type_->radial_grid());
            
            std::vector<double> enu_core(atom_type_->num_core_levels());
    
            for (int ist = 0; ist < atom_type_->num_core_levels(); ist++)
                enu_core[ist] = -1.0 * atom_type_->zn() / 2 / pow(atom_type_->atomic_level(ist).n, 2);
            
            memset(&rho[0], 0, rho.size() * sizeof(double));
            #pragma omp parallel default(shared)
            {
                std::vector<double> rho_t(rho.size());
                memset(&rho_t[0], 0, rho.size() * sizeof(double));
                std::vector<double> p;
                
                #pragma omp for
                for (int ist = 0; ist < atom_type_->num_core_levels(); ist++)
                {
                    solver.bound_state(atom_type_->atomic_level(ist).n, atom_type_->atomic_level(ist).l, 
                                       spherical_potential_, enu_core[ist], p);
                
                    for (int i = 0; i < atom_type_->radial_grid().size(); i++)
                        rho_t[i] += atom_type_->atomic_level(ist).occupancy * 
                                    pow(y00 * p[i] / atom_type_->radial_grid(i), 2);
                }

                #pragma omp critical
                for (int i = 0; i < rho.size(); i++)
                    rho[i] += rho_t[i];
            } 
                
            core_charge_density_ = rho.data_points();
            rho.interpolate();

            Spline<double> rho_mt(atom_type_->num_mt_points(), atom_type_->radial_grid());
            rho_mt.interpolate(core_charge_density_);

            core_leakage_ = fourpi * (rho.integrate(2) - rho_mt.integrate(2));

            core_eval_sum_ = 0.0;
            for (int ist = 0; ist < atom_type_->num_core_levels(); ist++)
                core_eval_sum_ += enu_core[ist] * atom_type_->atomic_level(ist).occupancy;

         }

         double core_charge_density(int ir)
         {
             assert(ir >= 0 && ir < (int)core_charge_density_.size());

             return core_charge_density_[ir];
         }

         inline AtomType* atom_type()
         {
             return atom_type_;
         }

         inline double core_eval_sum()
         {
             return core_eval_sum_;
         }

         inline double core_leakage()
         {
             return core_leakage_;
         }
};

};

#endif // __ATOM_SYMMETRY_CLASS_H__
