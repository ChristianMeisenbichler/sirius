#include <sirius.h>

void test1(double x0, double x1, int m, double exact_result)
{
    sirius::RadialGrid r(sirius::exponential_grid, 5000, x0, x1);
    sirius::Spline<double> s(5000, r);
    
    for (int i = 0; i < 5000; i++)
        s[i] = sin(r[i]);
    
    s.interpolate();
    
    double d = s.integrate(m);
    printf("power : %i   numerical result : %18.12f   exact_result : %18.12f   difference : %18.12f\n", m, d, exact_result, fabs(d - exact_result));
}

void test2(double x0, double x1)
{
    sirius::RadialGrid r(sirius::exponential_grid, 5000, x0, x1);
    sirius::Spline<double> s(5000, r);
    
    for (int i = 0; i < 5000; i++)
        s[i] = exp(r[i]);
    
    s.interpolate();
    
    std::cout << s[0] << " " << s.deriv(0, 0) << " " << s.deriv(1, 0) << " " << s.deriv(2, 0) << std::endl;
    std::cout << s[4999] << " " << s.deriv(0, 4999) << " " << s.deriv(1, 4999) << " " << s.deriv(2, 4999) << std::endl;

 
}

int main(int argn, char **argv)
{
    test1(0.1, 7.13, 0, 0.3326313127230704);
    test1(0.1, 7.13, 1, -3.973877090504168);
    test1(0.1, 7.13, 2, -23.66503552796384);
    test1(0.1, 7.13, 3, -101.989998166403);
    test1(0.1, 7.13, 4, -341.6457111811293);
    test1(0.1, 7.13, -1, 1.367605245879218);
    test1(0.1, 7.13, -2, 2.710875755556171);
    test1(0.1, 7.13, -3, 9.22907091561693);
    test1(0.1, 7.13, -4, 49.40653515725798);
    test1(0.1, 7.13, -5, 331.7312413927384);
 
    test2(0.1, 2.0);
    

}
