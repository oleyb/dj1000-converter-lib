#include "dj1000/post_geometry_filter.hpp"

#include <cassert>
#include <iostream>
#include <vector>

int main() {
    {
        std::vector<double> plane = {220.0, 150.0, 100.0};
        dj1000::apply_post_geometry_delta_filter(plane, 3, 1);
        assert(plane == std::vector<double>({220.0, 100.0, 100.0}));
    }

    {
        std::vector<double> plane = {100.0, 150.0, 220.0};
        dj1000::apply_post_geometry_delta_filter(plane, 3, 1);
        assert(plane == std::vector<double>({100.0, 100.0, 220.0}));
    }

    {
        std::vector<double> plane = {159.0, 100.0, 100.0};
        dj1000::apply_post_geometry_delta_filter(plane, 3, 1);
        assert(plane == std::vector<double>({159.0, 100.0, 100.0}));
    }

    {
        std::vector<double> delta0 = {220.0, 150.0, 100.0};
        std::vector<double> delta2 = {100.0, 150.0, 220.0};
        dj1000::apply_post_geometry_delta_filters(delta0, delta2, 3, 1);
        assert(delta0 == std::vector<double>({220.0, 100.0, 100.0}));
        assert(delta2 == std::vector<double>({100.0, 100.0, 220.0}));
    }

    std::cout << "test_post_geometry_filter passed\n";
    return 0;
}
