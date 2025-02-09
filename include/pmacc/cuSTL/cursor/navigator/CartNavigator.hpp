/* Copyright 2013-2021 Heiko Burau, Rene Widera
 *
 * This file is part of PMacc.
 *
 * PMacc is free software: you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License or
 * the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PMacc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License and the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * and the GNU Lesser General Public License along with PMacc.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "pmacc/cuSTL/cursor/traits.hpp"
#include "pmacc/math/vector/Int.hpp"
#include "tag.hpp"


namespace pmacc
{
    namespace cursor
    {
        template<int T_dim>
        class CartNavigator
        {
        public:
            using tag = tag::CartNavigator;
            static constexpr int dim = T_dim;

        private:
            math::Int<dim> factor;

        public:
            HDINLINE
            CartNavigator(math::Int<dim> factor) : factor(factor)
            {
            }

            template<typename Data>
            HDINLINE Data operator()(const Data& data, const math::Int<dim>& jump) const
            {
                auto* result = (char*) data;
                result += pmacc::math::dot(jump, this->factor);
                return (Data) result;
            }

            HDINLINE
            const math::Int<dim>& getFactor() const
            {
                return factor;
            }
        };

        namespace traits
        {
            template<int T_dim>
            struct dim<CartNavigator<T_dim>>
            {
                static constexpr int value = T_dim;
            };

        } // namespace traits

    } // namespace cursor
} // namespace pmacc
