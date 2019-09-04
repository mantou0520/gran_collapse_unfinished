/************************************************************************
 * MechSys - Open Library for Mechanical Systems                        *
 *                                                                      *
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * any later version.                                                   *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the         *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program. If not, see <http://www.gnu.org/licenses/>  *
 ************************************************************************/

#include <math.h>
#include <random>

#include <gsl/gsl_linalg.h>
// MechSys
#include <mechsys/dem/domain.h>
#include <mechsys/util/fatal.h>
#include <mechsys/util/util.h>
#include <mechsys/mesh/unstructured.h>
#include <mechsys/mesh/structured.h>
#include <mechsys/linalg/matvec.h>

using std::cout;
using std::endl;

int main(int argc, char **argv) try
{   
    
    if (argc<2) throw new Fatal("This program must be called with one argument: the name of the data input file without the '.inp' suffix.\nExample:\t %s filekey\n",argv[0]);
	
    // Setting number of CPUs
    size_t Nproc = 1;
    if (argc>=3) Nproc = atoi(argv[2]);

    String filekey  (argv[1]);
    String filename (filekey+".inp");
    if (!Util::FileExists(filename)) throw new Fatal("File <%s> not found",filename.CStr());
    ifstream infile(filename.CStr());
    
    String CrossSection;// Shape of the cross-section of the column
    String ptype;       // Particle type 
    String test;       // Test type 
    bool   Cohesion;    // Decide if coheison is going to be simulated
    double fraction;    // Fraction of particles to be generated
    double Kn;          // Normal stiffness
    double Kt;          // Tangential stiffness
    double Gn;          // Normal dissipative coefficient
    double Gt;          // Tangential dissipative coefficient
    double Mu;          // Microscopic friction coefficient
	double Muw;         // Frictional coefficient of the bottom wall
    double Bn;          // Cohesion normal stiffness
    double Bt;          // Cohesion tangential stiffness
    double Bm;          // Cohesion torque stiffness
    double Eps;         // Threshold for breking bonds
    double R;           // Spheroradius
    size_t seed;        // Seed of the ramdon generator
    double dt;          // Time step
    double dtOut;       // Time step for output
    double Lx;          // Lx
    double Ly;          // Ly
    double Lz;          // Lz
    size_t scalingx;    // scalingx
    size_t scalingy;    // scalingy
    size_t scalingz;    // scalingz
    size_t plane_x;     // the scaling of the size of the plane in x direction, how many particles per unit length
    size_t plane_y;     // the scaling of the size of the plane in y direction, how many particles per unit length
    double rho;         // rho
    double Tf;          // Final time for the test
    {
	    infile >> CrossSection;     infile.ignore(200,'\n');
	    infile >> ptype;     infile.ignore(200,'\n');
	    infile >> test;     infile.ignore(200,'\n');
        infile >> Cohesion;     infile.ignore(200,'\n');
        infile >> fraction;     infile.ignore(200,'\n');
        infile >> Kn;           infile.ignore(200,'\n');
        infile >> Kt;           infile.ignore(200,'\n');
        infile >> Gn;           infile.ignore(200,'\n');
        infile >> Gt;           infile.ignore(200,'\n');
        infile >> Mu;           infile.ignore(200,'\n');
		infile >> Muw;           infile.ignore(200,'\n');
        infile >> Bn;           infile.ignore(200,'\n');
        infile >> Bt;           infile.ignore(200,'\n');
        infile >> Bm;           infile.ignore(200,'\n');
        infile >> Eps;          infile.ignore(200,'\n');
        infile >> R;            infile.ignore(200,'\n');
        infile >> seed;         infile.ignore(200,'\n');
        infile >> dt;           infile.ignore(200,'\n');
        infile >> dtOut;        infile.ignore(200,'\n');
        infile >> Lx;           infile.ignore(200,'\n');
        infile >> Ly;           infile.ignore(200,'\n');
        infile >> Lz;           infile.ignore(200,'\n');
	    infile >> scalingx;     infile.ignore(200,'\n');
	    infile >> scalingy;     infile.ignore(200,'\n');
	    infile >> scalingz;     infile.ignore(200,'\n');
	    infile >> plane_x;      infile.ignore(200,'\n');
	    infile >> plane_y;      infile.ignore(200,'\n');
        infile >> rho;          infile.ignore(200,'\n');
        infile >> Tf;           infile.ignore(200,'\n');
    }

	
    //Some key parameters
    size_t Nx = size_t(Lx*scalingx);  //Division of the rectangular box
    size_t Ny = size_t(Ly*scalingy);
    size_t Nz = size_t(Lz*scalingz);
    Kn = Kn/(scalingx*scalingy); //Stiffness constant for particles
    Kt = Kt/(scalingx*scalingy);
    // domain
    DEM::Domain d;

    //Add the granular column
    if (ptype=="voronoi" || ptype=="Voronoi") d.AddVoroPack(-1,R,Lx,Ly,Lz,Nx,Ny,Nz,rho,Cohesion/*no cohesion*/,true/*periodic construction for angularity*/,seed,fraction);
	else if (ptype=="sphereboxnormal") 
    {
        Vec3_t Xmin(-0.5*Lx,-0.5*Ly,-0.5*Lz);
        Vec3_t Xmax = -Xmin;
        d.GenSpheresBox (-1, Xmin, Xmax, R, rho, "Normal", seed, fraction, Eps);
    }
    else if (ptype=="sphereboxhcp") 
    {
        Vec3_t Xmin(-0.5*Lx,-0.5*Ly,-0.5*Lz);
        Vec3_t Xmax = -Xmin;
        d.GenSpheresBox (-1, Xmin, Xmax, R, rho, "HCP", seed, fraction, Eps);
    }
    else if (ptype=="cube" || ptype=="Cube")
    {
        //std::random_device rd;  //Will be used to obtain a seed for the random number engine
        //std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
        //std::uniform_int_distribution<> dis(-0.001, 0.001);
        
        Vec3_t axis0(OrthoSys::e0); // rotation of face
        Vec3_t axis1(OrthoSys::e1); // rotation of face
        double Cf = 10.0;
        
        // estimate the number of particles we need
		size_t num_of_particles = scalingx*scalingy*scalingz*Lx*Ly*Lz;
		double cube_size = 1.0/scalingx;
		double delta_march = cube_size*sqrt(3.0);
		// position of the first cube
		Vec3_t X_cube (-0.5*Lx+delta_march, -0.5*Ly+delta_march, -0.5*Lz+delta_march);
		d.AddCube(-1, X_cube, R, cube_size, rho);
		// go over all the particles to 
		for (size_t np=1;np<num_of_particles;np++)
		{
			if (X_cube(1)<0.5*Ly-delta_march)
			{
				X_cube(1) = X_cube(1) + delta_march;
				d.AddCube(-1, X_cube, R, cube_size, rho);
			}
			else
			{
				X_cube(0) = X_cube(0) + delta_march;
				X_cube(1) = -0.5*Ly+delta_march;
				if (X_cube(0) < 0.5*Lx-delta_march)
				{
					d.AddCube(-1, X_cube, R, cube_size, rho);
				}
				else
				{
					X_cube(0) = -0.5*Lx+delta_march;
					X_cube(2) = X_cube(2)+delta_march;
					d.AddCube(-1, X_cube, R, cube_size, rho);
				}
			}
			// X_cube(2) = X_cube(2) + delta_march;
			// d.AddCube(-1, X_cube, R, cube_size, rho);
				
		}
		
		d.AddPlane (-11, Vec3_t(Lx/2.0,0.0,0.0),  R, Cf*Lz, Ly, 1.0, M_PI/2.0, &axis1);
        d.AddPlane (-12, Vec3_t(-Lx/2.0,0.0,0.0), R, Cf*Lz, Ly, 1.0, 3.0*M_PI/2.0, &axis1);
        d.AddPlane (-13, Vec3_t(0.0,Ly/2.0,0.0),  R, Lx, Cf*Lz, 1.0, 3.0*M_PI/2.0, &axis0);
        d.AddPlane (-14, Vec3_t(0.0,-Ly/2.0,0.0), R, Lx, Cf*Lz, 1.0, M_PI/2.0, &axis0);
        d.AddPlane (-15, Vec3_t(0.0,0.0,-Lz/2.0), R, Lx, Ly, 1.0, M_PI, &axis0);
        d.GetParticle(-11)->FixVeloc();
		d.GetParticle(-12)->FixVeloc();
		d.GetParticle(-13)->FixVeloc();
		d.GetParticle(-14)->FixVeloc();
		d.GetParticle(-15)->FixVeloc();
		
        for (size_t np=0;np<d.Particles.Size();np++)
    	{
            // d.Particles[np]->Tag = -1;
            d.Particles[np]->Ff = d.Particles[np]->Props.m*Vec3_t(0.0,0.0,-981.0);
            d.Particles[np]->Props.Kn = Kn; //normal stiffness
            d.Particles[np]->Props.Kt = Kt; //tangential stiffness
            d.Particles[np]->Props.Gn = Gn; //Restitution coefficient
            d.Particles[np]->Props.Mu = Mu; //Friction coefficient
            
    	}
    	
        // solve to get random packed cubes
        dt = 0.5*d.CriticalDt(); //Calculating time step
        d.Alpha = R; //Verlet distance
		//d.WriteXDMF("test");
        d.Solve(/*tf*/0.5*Tf, dt, /*dtOut*/0.5*Tf/20, NULL, NULL, "drop_cubes", 2, Nproc);
		d.Save("stage_1");

		
		for (size_t np=0;np<d.Particles.Size();np++)
		{
        	if (d.Particles[np]->x(0) > 0.5*Lx || d.Particles[np]->x(0) < -0.5*Lx || d.Particles[np]->x(1) > 0.5*Ly || d.Particles[np]->x(1) < -0.5*Ly || d.Particles[np]->x(2) > 0.5*Lz || d.Particles[np]->x(2) < -0.5*Lz)
        	{
           	 	d.Particles[np]->Tag = 10;
        	}
		}
		Array<int> delpar0;
    	Array<int> delpar1;
		Array<int> delpar2;
		Array<int> delpar3;
		Array<int> delpar4;
		Array<int> delpar5;
		
    	delpar0.Push(10);
    	d.DelParticles(delpar0);

        delpar1.Push(-11);
        d.DelParticles(delpar1);

		delpar2.Push(-12);
        d.DelParticles(delpar2);

		delpar3.Push(-13);
        d.DelParticles(delpar3);

		delpar4.Push(-14);
        d.DelParticles(delpar4);

		delpar5.Push(-15);
        d.DelParticles(delpar5);


    }
    else throw new Fatal("Packing for particle type not implemented yet");
	
    //Determinaning the bounding box
    Vec3_t Xmin,Xmax;
    d.BoundingBox(Xmin,Xmax);

    //Adding plate at the base of the column
    d.AddPlane(-2,Vec3_t(0.0,0.0,Xmin(2)-R),R,plane_x*Lz,plane_y*Lz,rho);

    //Fixing the Plane so it does not move (plane tag is -2)
    d.GetParticle(-2)->FixVeloc();

	// set the frictional coefficient for the bottom wall
    Dict D;
    D.Set(-2,"Kn, Kt, Gn, Mu",Kn, Kt, Gn, Muw);
    d.SetProps(D);
	
	Dict B;
	B.Set(-1,"Kn, Kt, Gn, Mu",Kn, Kt, Gn, Mu);
	d.SetProps(B);
	
    //Adding gravity to all particles as a fixed force and setting up the stiffness constant
    for (size_t np=0;np<d.Particles.Size();np++)
    {
		
        d.Particles[np]->Ff = d.Particles[np]->Props.m*Vec3_t(300.0,0.0,-981.0);
        d.Particles[np]->Props.Kn = Kn; //normal stiffness
        d.Particles[np]->Props.Kt = Kt; //tangential stiffness
        d.Particles[np]->Props.Gn = Gn; //Restitution coefficient
        d.Particles[np]->Props.Mu = Mu; //Friction coefficient
    }

	
	// Change the shape of cross-section
	if (CrossSection=="circle" || CrossSection=="Circle")
	{
		for (size_t np=0;np<d.Particles.Size();np++)
    	{
        	if (d.Particles[np]->x(0)*d.Particles[np]->x(0)+d.Particles[np]->x(1)*d.Particles[np]->x(1)>=0.25*Lx*Ly)
        	{
           	 	d.Particles[np]->Tag = 10;
        	}
    	}
    	Array<int> delpar;
    	delpar.Push(10);
    	d.DelParticles(delpar);
	}
	else if (CrossSection=="right_triangle")
	{
		for (size_t np=0;np<d.Particles.Size();np++)
    	{
        	if (d.Particles[np]->x(1) > Ly/Lx* d.Particles[np]->x(0))
        	{
           	 	d.Particles[np]->Tag = 10;
        	}
    	}
    	Array<int> delpar;
    	delpar.Push(10);
    	d.DelParticles(delpar);
	}
	else if (CrossSection=="isoscele_triangle")
	{
		for (size_t np=0;np<d.Particles.Size();np++)
    	{
        	if ((d.Particles[np]->x(1) > 2*Ly/Lx* d.Particles[np]->x(0) + Ly/2) || (d.Particles[np]->x(1) > -2*Ly/Lx* d.Particles[np]->x(0) + Ly/2))
        	{
           	 	d.Particles[np]->Tag = 10;
        	}
    	}
    	Array<int> delpar;
    	delpar.Push(10);
    	d.DelParticles(delpar);
	}
	else if (CrossSection=="square" || CrossSection=="Square")
	{
		std::cout << "The cross-section is a square" << std::endl;
	}
	else throw new Fatal("Packing for particle type not implemented yet");

    // solve
    dt = 0.5*d.CriticalDt(); //Calculating time step
    d.Alpha = R; //Verlet distance
	//d.WriteXDMF("test");
    d.Solve(/*tf*/1.5*Tf, dt, /*dtOut*/dtOut, NULL, NULL, "column", 2, Nproc);
}
MECHSYS_CATCH