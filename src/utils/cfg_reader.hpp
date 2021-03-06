#ifndef L2FSIM_CFG_READER_HPP_
#define L2FSIM_CFG_READER_HPP_

#include <libconfig.h++>
#include <memory>

#include <aircraft.hpp>
#include <beeler_glider/beeler_glider.hpp>
#include <beeler_glider/beeler_glider_state.hpp>
#include <beeler_glider/beeler_glider_command.hpp>

#include <flight_zone.hpp>
#include <flat_zone.hpp>
#include <flat_thermal_soaring_zone.hpp>
#include <model/gp_model.hpp>

#include <stepper.hpp>
#include <euler_integrator.hpp>
#include <rk4_integrator.hpp>

#include <pilot.hpp>
#include <passive_pilot.hpp>
#include <heuristic_pilot.hpp>
#include <q_learning/q_learning_pilot.hpp>
//#include <uct/uct.hpp>
#include <optimistic/optimistic_pilot.hpp>

/**
 * @brief Configuration file reader
 *
 * @file cfg_reader.hpp
 * @version 1.0
 */

namespace L2Fsim {

struct cfg_reader {
    /**
     * @brief Constructor
     */
    cfg_reader() {}

    /**
     * @brief Display error
     *
     * Generic error message.
     * @param {std::string} method_name; name of the called method, for clarity when
     * debugging.
     */
    void error_at(std::string method_name) {
        std::cout << "Error: variable initialisation in config file. Config file reader stopped at "<< method_name << std::endl;
    }

    /**
     * @brief Read path
     *
     * Read the path to the state log file.
     */
    std::string read_st_log_path(const libconfig::Config &cfg) {
        if(cfg.exists("st_log_path")) {return cfg.lookup("st_log_path");}
        else {error_at("read_st_log_path");}
        return nullptr;
    }

    /**
     * @brief Read path
     *
     * Read the path to the environment log file.
     */
    std::string read_fz_log_path(const libconfig::Config &cfg) {
        if(cfg.exists("fz_log_path")) {return cfg.lookup("fz_log_path");}
        else {error_at("read_fz_log_path");}
        return nullptr;
    }

    /**
     * @brief Read time variables
     *
     * Read and initialise the time variables.
     */
    void read_time_variables(const libconfig::Config &cfg, double &t_lim, double &Dt, double &nb_dt) {
        if(cfg.lookupValue("limit_time",t_lim)
        && cfg.lookupValue("time_step_width",Dt)
        && cfg.lookupValue("nb_sub_time_step",nb_dt)) {/* nothing to do */}
        else {error_at("read_time_variables");}
    }

    /**
     * @brief Read environment
     *
     * Read and initialise an environment.
     */
    std::unique_ptr<flight_zone> read_environment(const libconfig::Config &cfg) {
        unsigned int sl=0;
        if(cfg.lookupValue("envt_selector", sl)) {
            switch(sl) {
            case 0: { // flat_zone
                double wx=0., wy=0.;
                if(cfg.lookupValue("wx", wx) && cfg.lookupValue("wy", wy)) {
                    return std::unique_ptr<flight_zone> (new flat_zone(wx,wy));
                } else {error_at("read_environment");}
            }
            case 1: { // flat_thermal_soaring_zone
                std::string sc_path = "config/fz_scenario.csv";
                std::string envt_cfg_path = "config/fz_cfg.csv";
                double noise_stddev=0.;
                if (cfg.lookupValue("th_scenario_path", sc_path) &&
                    cfg.lookupValue("envt_cfg_path", envt_cfg_path) &&
                    cfg.lookupValue("noise_stddev", noise_stddev)) {
                    return std::unique_ptr<flight_zone> (new flat_thermal_soaring_zone(sc_path,envt_cfg_path,noise_stddev));
                } else {error_at("read_environment");}
            }
            }
        }
        else {error_at("read_environment");}
        return std::unique_ptr<flight_zone> (nullptr);
    }

    /**
	 * @brief Read state
	 *
	 * Read and set the values of the state variables.
     */
    void read_state(
        const libconfig::Config &cfg,
        double &_x0,
        double &_y0,
        double &_z0,
        double &_V0,
        double &_gamma0,
        double &_khi0,
        double &_alpha0,
        double &_beta0,
        double &_sigma0,
        double &_mam)
    {
        if(cfg.lookupValue("x0", _x0)
        && cfg.lookupValue("y0", _y0)
        && cfg.lookupValue("z0", _z0)
        && cfg.lookupValue("V0", _V0)
        && cfg.exists("gamma0")
        && cfg.exists("khi0")
        && cfg.exists("alpha0")
        && cfg.exists("beta0")
        && cfg.exists("sigma0")
        && cfg.exists("maximum_angle_magnitude")) {
            _gamma0 = cfg.lookup("gamma0"); _gamma0 *= TO_RAD;
            _khi0 = cfg.lookup("khi0"); _khi0 *= TO_RAD;
            _alpha0 = cfg.lookup("alpha0"); _alpha0 *= TO_RAD;
            _beta0 = cfg.lookup("beta0"); _beta0 *= TO_RAD;
            _sigma0 = cfg.lookup("sigma0"); _sigma0 *= TO_RAD;
            _mam = cfg.lookup("maximum_angle_magnitude"); _mam *= TO_RAD;
        }
        else {error_at("read_state");}
    }

    /**
	 * @brief Read aircraft
	 *
	 * Read and initialise an aircraft.
     */
    std::unique_ptr<aircraft> read_aircraft(const libconfig::Config &cfg) {
        if(cfg.exists("aircraft_selector")) {
            unsigned int sl = cfg.lookup("aircraft_selector");
            switch(sl) {
            case 0: { // beeler_glider
                double x0=0., y0=0., z0=0., V0=0., gamma0=0., khi0=0., alpha0=0., beta0=0., sigma0=0., mam=0.;
                read_state(cfg,x0,y0,z0,V0,gamma0,khi0,alpha0,beta0,sigma0,mam);
                beeler_glider_state s(x0,y0,z0,V0,gamma0,khi0,alpha0,beta0,sigma0,mam);
                beeler_glider_command a;
                return std::unique_ptr<aircraft> (new beeler_glider(s,a));
            }
            default: {error_at("read_aircraft");}
            }
        }
        else {error_at("read_aircraft");}
        return std::unique_ptr<aircraft> (nullptr);
    }

    /**
     * @brief Read stepper
     *
     * Read and initialise a stepper.
     */
    std::unique_ptr<stepper> read_stepper(const libconfig::Config &cfg, const double sub_dt) {
        if(cfg.exists("stepper_selector")) {
            unsigned int sl = cfg.lookup("stepper_selector");
            switch(sl) {
            case 0: { // euler_integrator
                return std::unique_ptr<stepper> (new euler_integrator(sub_dt));
            }
            case 1: { // rk4_integrator
                return std::unique_ptr<stepper> (new rk4_integrator(sub_dt));
            }
            }
        }
        else {error_at("read_stepper");}
        return std::unique_ptr<stepper> (nullptr);
    }

    /**
     * @brief Read pilot
     *
     * Read and initialise a pilot.
     * @todo write a method for each case
     */
    std::unique_ptr<pilot> read_pilot(const libconfig::Config &cfg) {
        if(cfg.exists("pilot_selector")) {
            unsigned int sl = cfg.lookup("pilot_selector");
            switch(sl) {
            case 0: { // passive_pilot
                double arm = .1;
                if(cfg.lookupValue("angle_rate_magnitude",arm)) {
                    arm *= TO_RAD;
                    return std::unique_ptr<pilot> (new passive_pilot(arm));
                } else {error_at("read_pilot");}
            }
            case 1: { // heuristic_pilot
                double arm = .1, kd = .01;
                if(cfg.lookupValue("angle_rate_magnitude",arm)
                && cfg.lookupValue("kdalpha",kd)) {
                    arm *= TO_RAD;
                    return std::unique_ptr<pilot> (new heuristic_pilot(arm,kd));
                } else {error_at("read_pilot");}
            }
            case 2: { // q_learning_pilot
                double arm=.1, kd=.01, ep=.1, lr=.01, df=.9;
                if(cfg.lookupValue("angle_rate_magnitude",arm)
                && cfg.lookupValue("kdalpha",kd)
                && cfg.lookupValue("q_epsilon",ep)
                && cfg.lookupValue("q_learning_rate",lr)
                && cfg.lookupValue("q_discount_factor",df))
                {
                    arm *= TO_RAD;
                    return std::unique_ptr<pilot> (new q_learning_pilot(arm,kd,ep,lr,df));
                } else {error_at("read_pilot");}
            }
            case 3: { // uct
                std::string sc_path, envt_cfg_path;
                double noise_stddev=0., arm=1., kd=.01, pr=.7, dt=.1, sdt=.1, df=.9;
                unsigned int hz=100, bd=1000, dfplselect=0;
                if(cfg.lookupValue("th_scenario_path", sc_path)
                && cfg.lookupValue("envt_cfg_path", envt_cfg_path)
                && cfg.lookupValue("noise_stddev", noise_stddev)
                && cfg.lookupValue("angle_rate_magnitude",arm)
                && cfg.lookupValue("kdalpha",kd)
                && cfg.lookupValue("uct_parameter",pr)
                && cfg.lookupValue("uct_time_step_width",dt)
                && cfg.lookupValue("uct_sub_time_step_width",sdt)
                && cfg.lookupValue("uct_discount_factor",df)
                && cfg.lookupValue("uct_horizon",hz)
                && cfg.lookupValue("uct_budget",bd)
                && cfg.lookupValue("uct_default_policy_selector",dfplselect))
                {
                    /*
                    double x0=0., y0=0., z0=0., V0=0., gamma0=0., khi0=0., alpha0=0., beta0=0., sigma0=0., mam=0.;
                    read_state(cfg,x0,y0,z0,V0,gamma0,khi0,alpha0,beta0,sigma0,mam);
                    beeler_glider_state s(x0,y0,z0,V0,gamma0,khi0,alpha0,beta0,sigma0,mam);
                    beeler_glider_command a;
                    beeler_glider ac_model(s,a);
                    arm *= TO_RAD;
					*/
                    /* TRM
                    return std::unique_ptr<pilot> (
                        new uct_pilot(
                            ac_model,
                            sc_path, envt_cfg_path, noise_stddev, // flat_thermal_soaring_zone parameters
                            euler_integrator::transition_function,
                            arm, kd, pr, dt, sdt, df, hz, bd, dfplselect
                        )
                    );
                    */
                    /*
                    return std::unique_ptr<pilot> (
                        new uct<
                            beeler_glider_state,
                            beeler_glider_action,
                            TODO
                        > (
                            ac_model,
                            sc_path, envt_cfg_path, noise_stddev, // flat_thermal_soaring_zone parameters
                            euler_integrator::transition_function,
                            arm, kd, pr, dt, sdt, df, hz, bd, dfplselect
                        )
                    );
                    */
                    error_at("read_pilot");//TRM
                } else {error_at("read_pilot");}
            }
            case 4: { // optimistic_pilot
                std::string sc_path, envt_cfg_path;
                double noise_stddev=0., arm=1., kd=.01, dt=.1, sdt=.1, df=.9;
                unsigned int bd=1000;
                if(cfg.lookupValue("th_scenario_path", sc_path)
                && cfg.lookupValue("envt_cfg_path", envt_cfg_path)
                && cfg.lookupValue("noise_stddev", noise_stddev)
                && cfg.lookupValue("angle_rate_magnitude",arm)
                && cfg.lookupValue("kdalpha",kd)
                && cfg.lookupValue("opt_time_step_width",dt)
                && cfg.lookupValue("opt_sub_time_step_width",sdt)
                && cfg.lookupValue("opt_discount_factor",df)
                && cfg.lookupValue("opt_budget",bd))
				{
                    double x0=0., y0=0., z0=0., V0=0., gamma0=0., khi0=0., alpha0=0., beta0=0., sigma0=0., mam=0.;
                    read_state(cfg,x0,y0,z0,V0,gamma0,khi0,alpha0,beta0,sigma0,mam);
                    beeler_glider_state s(x0,y0,z0,V0,gamma0,khi0,alpha0,beta0,sigma0,mam);
                    beeler_glider_command a;
                    beeler_glider ac_model(s,a);
                    arm *= TO_RAD;

                    return std::unique_ptr<pilot> (
						new optimistic_pilot(
							ac_model,
							sc_path, envt_cfg_path, noise_stddev, // flat_thermal_soaring_zone parameters
                        	arm, kd, dt, sdt, df, bd
						));
                } else {error_at("read_pilot");}
            }
            }
        }
        else {error_at("read_pilot");}
        return std::unique_ptr<pilot> (nullptr);
    }
};

}

#endif // L2FSIM_CFG_READER_HPP_
