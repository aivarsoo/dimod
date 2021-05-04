// Copyright 2021 D-Wave Systems Inc.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#include <iostream>

#include "../Catch2/single_include/catch2/catch.hpp"
#include "dimod/quadratic_model.h"

namespace dimod {

TEMPLATE_TEST_CASE_SIG("Scenario: BinaryQuadraticModel tests", "[qmbase][bqm]",
                       ((typename Bias, Vartype vartype), Bias, vartype),
                       (double, Vartype::BINARY), (double, Vartype::SPIN)) {
    GIVEN("an empty BQM") {
        auto bqm = BinaryQuadraticModel<Bias>(vartype);

        WHEN("the bqm is resized") {
            bqm.resize(10);

            THEN("it will have the correct number of variables with 0 bias") {
                REQUIRE(bqm.num_variables() == 10);
                REQUIRE(bqm.num_interactions() == 0);
                for (auto v = 0u; v < bqm.num_variables(); ++v) {
                    REQUIRE(bqm.linear(v) == 0);
                }
            }
        }
    }

    GIVEN("a BQM constructed from a dense array") {
        float Q[9] = {1, 0, 3, 2, 1, 0, 1, 0, 0};
        int num_variables = 3;

        auto bqm = BinaryQuadraticModel<Bias>(Q, num_variables, vartype);

        THEN("it handles the diagonal according to its vartype") {
            REQUIRE(bqm.num_variables() == 3);

            if (bqm.vartype() == Vartype::SPIN) {
                REQUIRE(bqm.linear(0) == 0);
                REQUIRE(bqm.linear(1) == 0);
                REQUIRE(bqm.linear(2) == 0);
                REQUIRE(bqm.offset() == 2);
            } else {
                REQUIRE(bqm.vartype() == Vartype::BINARY);
                REQUIRE(bqm.linear(0) == 1);
                REQUIRE(bqm.linear(1) == 1);
                REQUIRE(bqm.linear(2) == 0);
                REQUIRE(bqm.offset() == 0);
            }
        }

        THEN("it gets its quadratic from the off-diagonal") {
            REQUIRE(bqm.num_interactions() == 2);

            // test both forward and backward
            REQUIRE(bqm.quadratic(0, 1) == 2);
            REQUIRE(bqm.quadratic(1, 0) == 2);
            REQUIRE(bqm.quadratic(0, 2) == 4);
            REQUIRE(bqm.quadratic(2, 0) == 4);
            REQUIRE(bqm.quadratic(1, 2) == 0);
            REQUIRE(bqm.quadratic(2, 1) == 0);

            // ignores 0s
            REQUIRE_THROWS_AS(bqm.quadratic_at(1, 2), std::out_of_range);
            REQUIRE_THROWS_AS(bqm.quadratic_at(2, 1), std::out_of_range);
        }

        THEN("we can iterate over the neighborhood") {
            auto span = bqm.neighborhood(0);
            auto pairs = std::vector<std::pair<std::size_t, Bias>>(span.first,
                                                                   span.second);

            REQUIRE(pairs[0].first == 1);
            REQUIRE(pairs[0].second == 2);
            REQUIRE(pairs[1].first == 2);
            REQUIRE(pairs[1].second == 4);
            REQUIRE(pairs.size() == 2);
        }
    }

    GIVEN("a BQM with five variables, two interactions and an offset") {
        auto bqm = BinaryQuadraticModel<Bias>(5, vartype);
        bqm.linear(0) = 1;
        bqm.linear(1) = -3.25;
        bqm.linear(2) = 0;
        bqm.linear(3) = 3;
        bqm.linear(4) = -4.5;
        bqm.set_quadratic(0, 3, -1);
        bqm.set_quadratic(3, 1, 5.6);
        bqm.set_quadratic(0, 1, 1.6);
        bqm.offset() = -3.8;

        AND_GIVEN("the set of all possible five variable samples") {
            // there are smarter ways to do this but it's simple
            std::vector<std::vector<int>> spn_samples;
            std::vector<std::vector<int>> bin_samples;
            for (auto i = 0; i < 1 << bqm.num_variables(); ++i) {
                std::vector<int> bin_sample;
                std::vector<int> spn_sample;
                for (size_t v = 0; v < bqm.num_variables(); ++v) {
                    bin_sample.push_back((i >> v) & 1);
                    spn_sample.push_back(2 * ((i >> v) & 1) - 1);
                }

                bin_samples.push_back(bin_sample);
                spn_samples.push_back(spn_sample);
            }

            std::vector<double> energies;
            if (vartype == Vartype::SPIN) {
                for (auto& sample : spn_samples) {
                    energies.push_back(bqm.energy(sample));
                }
            } else {
                for (auto& sample : bin_samples) {
                    energies.push_back(bqm.energy(sample));
                }
            }

            WHEN("we change the vartype to spin") {
                bqm.change_vartype(Vartype::SPIN);

                THEN("the energies will match") {
                    for (size_t si = 0; si < energies.size(); ++si) {
                        REQUIRE(energies[si] ==
                                Approx(bqm.energy(spn_samples[si])));
                    }
                }
            }

            WHEN("we change the vartype to binary") {
                bqm.change_vartype(Vartype::BINARY);
                THEN("the energies will match") {
                    for (size_t si = 0; si < energies.size(); ++si) {
                        REQUIRE(energies[si] ==
                                Approx(bqm.energy(bin_samples[si])));
                    }
                }
            }
        }
    }
}

SCENARIO("Neighborhood can be manipulated") {
    GIVEN("An empty Neighborhood") {
        auto neighborhood = Neighborhood<float, size_t>();

        WHEN("some variables/biases are emplaced") {
            neighborhood.emplace_back(0, .5);
            neighborhood.emplace_back(1, 1.5);
            neighborhood.emplace_back(3, -3);

            THEN("we can retrieve the biases with .at()") {
                REQUIRE(neighborhood.size() == 3);
                REQUIRE(neighborhood.at(0) == .5);
                REQUIRE(neighborhood.at(1) == 1.5);
                REQUIRE(neighborhood.at(3) == -3);

                // should throw an error
                REQUIRE_THROWS_AS(neighborhood.at(2), std::out_of_range);
                REQUIRE(neighborhood.size() == 3);
            }

            THEN("we can retrieve the biases with []") {
                REQUIRE(neighborhood.size() == 3);
                REQUIRE(neighborhood[0] == .5);
                REQUIRE(neighborhood[1] == 1.5);
                REQUIRE(neighborhood[2] == 0);  // created
                REQUIRE(neighborhood[3] == -3);
                REQUIRE(neighborhood.size() == 4);  // since 2 was inserted
            }

            THEN("we can retrieve the biases with .get()") {
                REQUIRE(neighborhood.size() == 3);
                REQUIRE(neighborhood.get(0) == .5);
                REQUIRE(neighborhood.get(1) == 1.5);
                REQUIRE(neighborhood.get(1, 2) == 1.5);  // use real value
                REQUIRE(neighborhood.get(2) == 0);
                REQUIRE(neighborhood.get(2, 1.5) == 1.5);  // use default
                REQUIRE(neighborhood.at(3) == -3);
                REQUIRE(neighborhood.size() == 3);  // should not change
            }

            THEN("we can modify the biases with []") {
                neighborhood[0] += 7;
                neighborhood[2] -= 3;

                REQUIRE(neighborhood.at(0) == 7.5);
                REQUIRE(neighborhood.at(2) == -3);
            }

            THEN("we can create a vector from the neighborhood") {
                std::vector<std::pair<size_t, float>> pairs(
                        neighborhood.begin(), neighborhood.end());

                REQUIRE(pairs[0].first == 0);
                REQUIRE(pairs[0].second == .5);
                REQUIRE(pairs[1].first == 1);
                REQUIRE(pairs[1].second == 1.5);
                REQUIRE(pairs[2].first == 3);
                REQUIRE(pairs[2].second == -3);
            }

            THEN("we can create a vector from the const neighborhood") {
                std::vector<std::pair<size_t, float>> pairs(
                        neighborhood.cbegin(), neighborhood.cend());

                REQUIRE(pairs[0].first == 0);
                REQUIRE(pairs[0].second == .5);
                REQUIRE(pairs[1].first == 1);
                REQUIRE(pairs[1].second == 1.5);
                REQUIRE(pairs[2].first == 3);
                REQUIRE(pairs[2].second == -3);
            }

            THEN("we can modify the biases via the iterator") {
                auto it = neighborhood.begin();

                (*it).second = 18;
                REQUIRE(neighborhood.at(0) == 18);

                it++;
                (*it).second = -48;
                REQUIRE(neighborhood.at(1) == -48);

                // it++;
                // it->second += 1;
            }
        }
    }
}
}  // namespace dimod