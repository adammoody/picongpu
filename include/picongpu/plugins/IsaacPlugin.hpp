/*
 * Copyright 2013-2021 Alexander Matthes, Pawel Ordyna
 *
 * This file is part of PIConGPU.
 *
 * PIConGPU is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PIConGPU is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PIConGPU.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

// Needs to be the very first
#include "picongpu/plugins/ILightweightPlugin.hpp"

#include <pmacc/dataManagement/DataConnector.hpp>
#include <pmacc/static_assert.hpp>

#include <boost/fusion/include/mpl.hpp>

#define ISAAC_IDX_TYPE cupla::IdxType
#include <boost/fusion/container/list.hpp>
#include <boost/fusion/container/list/list_fwd.hpp>
#include <boost/fusion/include/as_list.hpp>
#include <boost/fusion/include/list.hpp>
#include <boost/fusion/include/list_fwd.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/vector.hpp>

#include <limits>

#include <isaac.hpp>

namespace picongpu
{
    namespace isaacP
    {
        using namespace pmacc;
        using namespace ::isaac;

        ISAAC_NO_HOST_DEVICE_WARNING
        template<typename FieldType>
        class TFieldSource
        {
        public:
            static const size_t featureDim = 3;
            static const ISAAC_IDX_TYPE guardSize = 0;
            // static const bool hasGuard = bmpl::not_<boost::is_same<FieldType, FieldJ>>::value;
            static const bool persistent = bmpl::not_<boost::is_same<FieldType, FieldJ>>::value;
            typename FieldType::DataBoxType shifted;
            MappingDesc* cellDescription;
            TFieldSource() : cellDescription(nullptr)
            {
            }

            void init(MappingDesc* cellDescription)
            {
                this->cellDescription = cellDescription;
            }

            static std::string getName()
            {
                return FieldType::getName() + std::string(" field");
            }

            void update(bool enabled, void* pointer)
            {
                if(enabled)
                {
                    DataConnector& dc = Environment<simDim>::get().DataConnector();
                    auto pField = dc.get<FieldType>(FieldType::getName(), true);
                    DataSpace<simDim> guarding = SuperCellSize::toRT() * cellDescription->getGuardingSuperCells();

                    typename FieldType::DataBoxType dataBox = pField->getDeviceDataBox();
                    shifted = dataBox.shift(guarding);
                    /* avoid deadlock between not finished pmacc tasks and potential blocking operations
                     * within ISAAC
                     */
                    __getTransactionEvent().waitForFinished();
                }
            }

            ISAAC_NO_HOST_DEVICE_WARNING
            ISAAC_HOST_DEVICE_INLINE isaac_float_dim<featureDim> operator[](const isaac_int3& nIndex) const
            {
                auto value = shifted[nIndex.z][nIndex.y][nIndex.x];
                return isaac_float_dim<featureDim>(value.x(), value.y(), value.z());
            }
        };

        ISAAC_NO_HOST_DEVICE_WARNING
        template<typename FrameSolver, typename ParticleType, typename ParticleFilter>
        class TFieldSource<FieldTmpOperation<FrameSolver, ParticleType, ParticleFilter>>
        {
        public:
            static const size_t featureDim = 1;
            static const ISAAC_IDX_TYPE guardSize = 0;
            static const bool persistent = false;
            typename FieldTmp::DataBoxType shifted;
            MappingDesc* cellDescription;

            TFieldSource() : cellDescription(nullptr)
            {
            }

            void init(MappingDesc* cellDescription)
            {
                this->cellDescription = cellDescription;
            }

            static std::string getName()
            {
                return ParticleType::FrameType::getName() + std::string(" ") + ParticleFilter::getName()
                    + std::string(" ") + FrameSolver().getName();
            }

            void update(bool enabled, void* pointer)
            {
                if(enabled)
                {
                    uint32_t* currentStep = (uint32_t*) pointer;
                    DataConnector& dc = Environment<simDim>::get().DataConnector();

                    PMACC_CASSERT_MSG(_please_allocate_at_least_one_FieldTmp_in_memory_param, fieldTmpNumSlots > 0);
                    auto fieldTmp = dc.get<FieldTmp>(FieldTmp::getUniqueId(0), true);
                    auto particles = dc.get<ParticleType>(ParticleType::FrameType::getName(), true);

                    fieldTmp->getGridBuffer().getDeviceBuffer().setValue(FieldTmp::ValueType(0.0));
                    fieldTmp->template computeValue<CORE + BORDER, FrameSolver, ParticleFilter>(
                        *particles,
                        *currentStep);
                    EventTask fieldTmpEvent = fieldTmp->asyncCommunication(__getTransactionEvent());

                    __setTransactionEvent(fieldTmpEvent);
                    __getTransactionEvent().waitForFinished();

                    DataSpace<simDim> guarding = SuperCellSize::toRT() * cellDescription->getGuardingSuperCells();
                    typename FieldTmp::DataBoxType dataBox = fieldTmp->getDeviceDataBox();
                    shifted = dataBox.shift(guarding);
                }
            }

            ISAAC_NO_HOST_DEVICE_WARNING
            ISAAC_HOST_DEVICE_INLINE isaac_float_dim<featureDim> operator[](const isaac_int3& nIndex) const
            {
                auto value = shifted[nIndex.z][nIndex.y][nIndex.x];
                return isaac_float_dim<featureDim>(value.x());
            }
        };

        ISAAC_NO_HOST_DEVICE_WARNING
        template<typename FieldType>
        class TVectorFieldSource
        {
        public:
            static const size_t featureDim = 3;
            static const ISAAC_IDX_TYPE guardSize = 0;
            static const bool persistent = bmpl::not_<boost::is_same<FieldType, FieldJ>>::value;
            typename FieldType::DataBoxType shifted;
            MappingDesc* cellDescription;
            TVectorFieldSource() : cellDescription(nullptr)
            {
            }

            void init(MappingDesc* cellDescription)
            {
                this->cellDescription = cellDescription;
            }

            static std::string getName()
            {
                return FieldType::getName() + std::string(" vector field");
            }

            void update(bool enabled, void* pointer)
            {
                if(enabled)
                {
                    DataConnector& dc = Environment<simDim>::get().DataConnector();
                    auto pField = dc.get<FieldType>(FieldType::getName(), true);
                    DataSpace<simDim> guarding = SuperCellSize::toRT() * cellDescription->getGuardingSuperCells();

                    typename FieldType::DataBoxType dataBox = pField->getDeviceDataBox();
                    shifted = dataBox.shift(guarding);
                    /* avoid deadlock between not finished pmacc tasks and potential blocking operations
                     * within ISAAC
                     */
                    __getTransactionEvent().waitForFinished();
                }
            }

            ISAAC_NO_HOST_DEVICE_WARNING
            ISAAC_HOST_DEVICE_INLINE isaac_float_dim<featureDim> operator[](const isaac_int3& nIndex) const
            {
                auto value = shifted[nIndex.z][nIndex.y][nIndex.x];
                return isaac_float_dim<featureDim>(value.x(), value.y(), value.z());
            }
        };


        template<size_t featureDim, typename ParticlesBoxType>
        class ParticleIterator
        {
        public:
            using FramePtr = typename ParticlesBoxType::FramePtr;
            // size of the particle list
            size_t size;

            ISAAC_NO_HOST_DEVICE_WARNING
            ISAAC_HOST_DEVICE_INLINE ParticleIterator(
                size_t size,
                ParticlesBoxType pb,
                FramePtr firstFrame,
                int frameSize)
                : size(size)
                , pb(pb)
                , frame(firstFrame)
                , frameSize(frameSize)
                , i(0)
            {
            }

            ISAAC_HOST_DEVICE_INLINE void next()
            {
                // iterate particles look for next frame
                ++i;
                if(i >= frameSize)
                {
                    frame = pb.getNextFrame(frame);
                    i = 0;
                }
            }

            // returns current particle position
            ISAAC_HOST_DEVICE_INLINE isaac_float3 getPosition() const
            {
                auto const particle = frame[i];

                // storage number in the actual frame
                const auto frameCellNr = particle[localCellIdx_];

                // offset in the actual superCell = cell offset in the supercell
                const DataSpace<simDim> frameCellOffset(
                    DataSpaceOperations<simDim>::template map<MappingDesc::SuperCellSize>(frameCellNr));

                // added offsets
                float3_X const absoluteOffset(particle[position_] + float3_X(frameCellOffset));

                // calculate scaled position
                isaac_float3 const pos(
                    absoluteOffset.x() * (1._X / float_X(MappingDesc::SuperCellSize::x::value)),
                    absoluteOffset.y() * (1._X / float_X(MappingDesc::SuperCellSize::y::value)),
                    absoluteOffset.z() * (1._X / float_X(MappingDesc::SuperCellSize::z::value)));

                return pos;
            }

            // returns particle momentum as color attribute
            ISAAC_HOST_DEVICE_INLINE isaac_float_dim<featureDim> getAttribute() const
            {
                auto const particle = frame[i];
                float3_X const mom = particle[momentum_];
                return isaac_float_dim<featureDim>(mom[0], mom[1], mom[2]);
            }


            // returns constant radius
            ISAAC_HOST_DEVICE_INLINE isaac_float getRadius() const
            {
                return 0.2f;
            }


        private:
            ParticlesBoxType pb;
            FramePtr frame;
            int i;
            int frameSize;
        };


        ISAAC_NO_HOST_DEVICE_WARNING
        template<typename ParticlesType>
        class ParticleSource
        {
            using ParticlesBoxType = typename ParticlesType::ParticlesBoxType;
            using FramePtr = typename ParticlesBoxType::FramePtr;
            using FrameType = typename ParticlesBoxType::FrameType;

        public:
            static const size_t featureDim = 3;
            DataSpace<simDim> guarding;
            ISAAC_NO_HOST_DEVICE_WARNING
            ParticleSource()
            {
            }

            ISAAC_HOST_INLINE static std::string getName()
            {
                return ParticlesType::FrameType::getName() + std::string(" particle");
            }

            pmacc::memory::Array<ParticlesBoxType, 1> pb;

            void update(bool enabled, void* pointer)
            {
                // update movingWindow cells
                if(enabled)
                {
                    DataConnector& dc = Environment<>::get().DataConnector();
                    auto particles = dc.get<ParticlesType>(ParticlesType::FrameType::getName(), true);
                    pb[0] = particles->getDeviceParticlesBox();

                    guarding = GuardSize::toRT();
                }
            }

            // returns particleIterator with correct featureDim and cell specific particlebox
            ISAAC_NO_HOST_DEVICE_WARNING
            ISAAC_HOST_DEVICE_INLINE ParticleIterator<featureDim, ParticlesBoxType> getIterator(
                const isaac_uint3& local_grid_coord) const
            {
                constexpr uint32_t frameSize = pmacc::math::CT::volume<typename FrameType::SuperCellSize>::type::value;
                DataSpace<simDim> const superCellIdx(
                    local_grid_coord.x + guarding[0],
                    local_grid_coord.y + guarding[1],
                    local_grid_coord.z + guarding[2]);
                const auto& superCell = pb[0].getSuperCell(superCellIdx);
                size_t size = superCell.getNumParticles();
                FramePtr currentFrame = pb[0].getFirstFrame(superCellIdx);
                return ParticleIterator<featureDim, ParticlesBoxType>(size, pb[0], currentFrame, frameSize);
            }
        };

        template<typename T>
        struct Transformoperator
        {
            typedef TFieldSource<T> type;
        };
        template<typename T>
        struct VectorFieldTransformoperator
        {
            typedef TVectorFieldSource<T> type;
        };
        template<typename T>
        struct ParticleTransformoperator
        {
            typedef ParticleSource<T> type;
        };

        struct SourceInitIterator
        {
            template<typename TSource, typename TCellDescription>
            void operator()(const int I, TSource& s, TCellDescription& c) const
            {
                s.init(c);
            }
        };

        class IsaacPlugin : public ILightweightPlugin
        {
        public:
            static const ISAAC_IDX_TYPE textureDim = 1024;
            using SourceList = bmpl::
                transform<boost::fusion::result_of::as_list<Fields_Seq>::type, Transformoperator<bmpl::_1>>::type;
            using VectorFieldSourceList = bmpl::transform<
                boost::fusion::result_of::as_list<VectorFields_Seq>::type,
                VectorFieldTransformoperator<bmpl::_1>>::type;
            // create compile time particle list
            using ParticleList = bmpl::transform<
                boost::fusion::result_of::as_list<Particle_Seq>::type,
                ParticleTransformoperator<bmpl::_1>>::type;
            using VisualizationType = IsaacVisualization<
                cupla::AccHost,
                cupla::Acc,
                cupla::AccStream,
                cupla::KernelDim,
                SourceList,
                VectorFieldSourceList,
                ParticleList,
                textureDim,
#if(ISAAC_STEREO == 0)
                isaac::DefaultController,
                isaac::DefaultCompositor
#else
                isaac::StereoController,
#    if(ISAAC_STEREO == 1)
                isaac::StereoCompositorSideBySide<isaac::StereoController>
#    else
                isaac::StereoCompositorAnaglyph<isaac::StereoController, 0x000000FF, 0x00FFFF00>
#    endif
#endif
                >;
            VisualizationType* visualization;

            IsaacPlugin()
                : visualization(nullptr)
                , cellDescription(nullptr)
                , movingWindow(false)
                , renderInterval(1)
                , step(0)
                , drawingTime(0)
                , simulationTime(0)
                , cellCount(0)
                , particleCount(0)
                , lastNotify(0)
                , runSteps(-10)
                , timingsFileExist(0)
                , recording(false)
            {
                Environment<>::get().PluginConnector().registerPlugin(this);
            }

            std::string pluginGetName() const override
            {
                return "IsaacPlugin";
            }

            void writeTimes(int time)
            {
                if(rank == 0)
                {
                    int min = std::numeric_limits<int>::max();
                    int max = 0;
                    int average = 0;
                    int times[numProc];
                    MPI_Gather(&time, 1, MPI_INT, times, 1, MPI_INT, 0, MPI_COMM_WORLD);
                    for(int i = 0; i < numProc; i++)
                    {
                        min = (times[i] < min) ? times[i] : min;
                        max = (times[i] > max) ? times[i] : max;
                        average += times[i];
                    }
                    average /= numProc;
                    timingsFile << min << "," << max << "," << average << ",";
                }
                else
                {
                    MPI_Gather(&time, 1, MPI_INT, NULL, 0, MPI_INT, 0, MPI_COMM_WORLD);
                }
            }

            void benchmark(bool pause)
            {
                if(recording && !pause && runSteps >= 0)
                {
                    if(rank == 0)
                    {
                        json_t* feedback = json_object();
                        json_t* array = json_array();
                        if(runSteps < 360)
                        {
                            json_array_append_new(array, json_real(1.0));
                            json_array_append_new(array, json_real(0.0));
                            json_array_append_new(array, json_real(0.0));
                        }
                        else if(runSteps < 720)
                        {
                            json_array_append_new(array, json_real(0.0));
                            json_array_append_new(array, json_real(1.0));
                            json_array_append_new(array, json_real(0.0));
                        }
                        else if(runSteps < 1080)
                        {
                            json_array_append_new(array, json_real(0.0));
                            json_array_append_new(array, json_real(0.0));
                            json_array_append_new(array, json_real(1.0));
                        }
                        else
                        {
                            json_array_append_new(array, json_real(1.0));
                            json_array_append_new(array, json_real(1.0));
                            json_array_append_new(array, json_real(1.0));
                        }
                        json_array_append_new(array, json_real(1.0));
                        json_object_set_new(feedback, "rotation axis", array);
                        visualization->getCommunicator()->setMessage(feedback);

                        timingsFile << runSteps << ",";
                    }
                    writeTimes(simulationTime);
                    writeTimes(drawingTime);
                    writeTimes(visualization->kernelTime);
                    writeTimes(visualization->mergeTime);
                    writeTimes(visualization->videoSendTime);
                    writeTimes(visualization->copyTime);
                    writeTimes(visualization->sortingTime);
                    writeTimes(visualization->bufferTime);
                    writeTimes(visualization->advectionTime);
                    writeTimes(visualization->advectionBorderTime);
                    writeTimes(visualization->optimizationBufferTime);
                    timingsFile << "\n";

                    if(rank == 0 && timingsFile && runSteps == 1440)
                    {
                        timingsFile.close();
                        recording = false;
                    }
                }
            }

            void notify(uint32_t currentStep) override
            {
                if(recording)
                {
                    // guarantee for benchmarking run that all simulation related mpi communication is finished
                    __getTransactionEvent().waitForFinished();
                }
                simulationTime = getTicksUs() - lastNotify;
                step++;
                if(step >= renderInterval)
                {
                    step = 0;
                    bool pause = false;
                    do
                    {
                        // update of the position for moving window simulations
                        if(movingWindow)
                        {
                            Window window(MovingWindow::getInstance().getWindow(currentStep));
                            isaac_int3 position;
                            const SubGrid<simDim>& subGrid = Environment<simDim>::get().SubGrid();
                            GridController<simDim>& gc = Environment<simDim>::get().GridController();

                            for(ISAAC_IDX_TYPE i = 0; i < 3; ++i)
                            {
                                if(gc.getPosition()[1] == 0) // first gpu
                                {
                                    position[i] = isaac_int(window.localDimensions.offset[i])
                                        + isaac_int(window.localDimensions.size[i])
                                        - isaac_int(subGrid.getLocalDomain().size[i]);
                                }
                                else
                                {
                                    position[i] = isaac_int(window.localDimensions.offset[i]);
                                }
                            }
                            visualization->updatePosition(position);
                            visualization->updateBounding();

                            isaac::Neighbours<isaac_int> neighbourIds;
                            for(uint32_t exchange = 0u; exchange < 27; ++exchange)
                            {
                                neighbourIds.array[exchange] = gc.getCommunicator().ExchangeTypeToRank(exchange);
                            }
                            visualization->updateNeighbours(neighbourIds);
                        }
                        if(rank == 0 && visualization->kernelTime)
                        {
                            json_object_set_new(
                                visualization->getJsonMetaRoot(),
                                "time step",
                                json_integer(currentStep));
                            json_object_set_new(
                                visualization->getJsonMetaRoot(),
                                "drawing_time",
                                json_integer(drawingTime));
                            json_object_set_new(
                                visualization->getJsonMetaRoot(),
                                "simulation_time",
                                json_integer(simulationTime));
                            json_object_set_new(
                                visualization->getJsonMetaRoot(),
                                "cell count",
                                json_integer(cellCount));
                            json_object_set_new(
                                visualization->getJsonMetaRoot(),
                                "particle count",
                                json_integer(particleCount));
                        }
                        uint64_t start = getTicksUs();
                        json_t* meta = visualization->doVisualization(META_MASTER, &currentStep, !pause);
                        // json_t* meta = nullptr;
                        drawingTime = getTicksUs() - start;
                        benchmark(pause);
                        json_t* jsonPause = nullptr;
                        if(meta && (jsonPause = json_object_get(meta, "pause")) && json_boolean_value(jsonPause))
                            pause = !pause;
                        if(meta && json_integer_value(json_object_get(meta, "exit")))
                            exit(1);
                        json_t* js;
                        if(meta && (js = json_object_get(meta, "interval")))
                        {
                            renderInterval = math::max(int(1), int(json_integer_value(js)));
                            // Feedback for other clients than the changing one
                            if(rank == 0)
                                json_object_set_new(
                                    visualization->getJsonMetaRoot(),
                                    "interval",
                                    json_integer(renderInterval));
                        }
                        json_decref(meta);
                        if(directPause)
                        {
                            pause = true;
                            directPause = false;
                        }
                    } while(pause);
                }
                runSteps++;
                lastNotify = getTicksUs();
            }

            void pluginRegisterHelp(po::options_description& desc) override
            {
                /* register command line parameters for your plugin */
                desc.add_options()(
                    "isaac.period",
                    po::value<std::string>(&notifyPeriod),
                    "Enable IsaacPlugin [for each n-th step].")(
                    "isaac.name",
                    po::value<std::string>(&name)->default_value("default"),
                    "The name of the simulation. Default is \"default\".")(
                    "isaac.url",
                    po::value<std::string>(&url)->default_value("localhost"),
                    "The url of the isaac server to connect to. Default is \"localhost\".")(
                    "isaac.port",
                    po::value<uint16_t>(&port)->default_value(2460),
                    "The port of the isaac server to connect to. Default is 2460.")(
                    "isaac.width",
                    po::value<uint32_t>(&width)->default_value(1024),
                    "The width per isaac framebuffer. Default is 1024.")(
                    "isaac.height",
                    po::value<uint32_t>(&height)->default_value(768),
                    "The height per isaac framebuffer. Default is 768.")(
                    "isaac.directPause",
                    po::value<bool>(&directPause)->default_value(false),
                    "Direct pausing after starting simulation. Default is false.")(
                    "isaac.quality",
                    po::value<uint32_t>(&jpeg_quality)->default_value(90),
                    "JPEG quality. Default is 90.")(
                    "isaac.reconnect",
                    po::value<bool>(&reconnect)->default_value(true),
                    "Trying to reconnect every time an image is rendered if the connection is lost or could never "
                    "established at all.")(
                    "isaac.timingsFilename",
                    po::value<std::string>(&timingsFilename)->default_value(""),
                    "Filename for dumping ISAAC timings.");
            }

            void setMappingDescription(MappingDesc* cellDescription) override
            {
                this->cellDescription = cellDescription;
            }

        private:
            MappingDesc* cellDescription;
            std::string notifyPeriod;
            std::string url;
            std::string name;
            uint16_t port;
            uint32_t count;
            uint32_t width;
            uint32_t height;
            uint32_t jpeg_quality;
            int rank;
            int numProc;
            bool movingWindow;
            SourceList sources;
            VectorFieldSourceList vecFieldSources;
            ParticleList particleSources;
            /** render interval within the notify period
             *
             * render each n-th time step within an interval defined by notifyPeriod
             */
            uint32_t renderInterval;
            uint32_t step;
            int drawingTime;
            int simulationTime;
            bool directPause;
            int cellCount;
            int particleCount;
            uint64_t lastNotify;
            bool reconnect;

            // storage for timings and control variables
            bool timingsFileExist;
            bool recording;
            int runSteps;
            std::ofstream timingsFile;
            std::string timingsFilename;

            void pluginLoad() override
            {
                if(!notifyPeriod.empty())
                {
                    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
                    MPI_Comm_size(MPI_COMM_WORLD, &numProc);
                    if(MovingWindow::getInstance().isEnabled())
                        movingWindow = true;
                    isaac_float minCellSize = math::min(cellSize[0], math::min(cellSize[1], cellSize[2]));
                    isaac_float3 cellSizeFactor(
                        cellSize[0] / minCellSize,
                        cellSize[1] / minCellSize,
                        cellSize[2] / minCellSize);

                    const SubGrid<simDim>& subGrid = Environment<simDim>::get().SubGrid();

                    isaac_size2 framebuffer_size = {cupla::IdxType(width), cupla::IdxType(height)};

                    forEachParams(sources, SourceInitIterator(), cellDescription);
                    forEachParams(vecFieldSources, SourceInitIterator(), cellDescription);

                    isaac_size3 globalSize;
                    isaac_size3 localSize;
                    isaac_size3 particleSize;
                    isaac_size3 position;
                    for(ISAAC_IDX_TYPE i = 0; i < 3; ++i)
                    {
                        globalSize[i] = MovingWindow::getInstance().getWindow(0).globalDimensions.size[i];
                        localSize[i] = subGrid.getLocalDomain().size[i];
                        particleSize[i] = subGrid.getLocalDomain().size[i] / SuperCellSize::toRT()[i];
                        position[i] = subGrid.getLocalDomain().offset[i];
                    }
                    visualization = new VisualizationType(
                        cupla::manager::Device<cupla::AccHost>::get().current(),
                        cupla::manager::Device<cupla::AccDev>::get().current(),
                        cupla::manager::Stream<cupla::AccDev, cupla::AccStream>::get().stream(),
                        name,
                        0,
                        url,
                        port,
                        framebuffer_size,
                        globalSize,
                        localSize,
                        particleSize,
                        position,
                        sources,
                        vecFieldSources,
                        particleSources,
                        cellSizeFactor);
                    visualization->setJpegQuality(jpeg_quality);

                    if(rank == 0)
                    {
                        auto& gc = Environment<simDim>::get().GridController();

                        for(uint32_t exchange = 1u; exchange < 27; ++exchange)
                        {
                            int neighborRank = gc.getCommunicator().ExchangeTypeToRank(exchange);
                            std::cout << exchange << ": " << neighborRank << std::endl;
                        }
                    }

                    isaac::Neighbours<isaac_int> neighbourIds;
                    auto& gc = Environment<simDim>::get().GridController();

                    for(uint32_t exchange = 0u; exchange < 27; ++exchange)
                    {
                        neighbourIds.array[exchange] = gc.getCommunicator().ExchangeTypeToRank(exchange);
                    }
                    visualization->updateNeighbours(neighbourIds);
                    // Defining the later periodicly sent meta data
                    if(rank == 0)
                    {
                        json_object_set_new(visualization->getJsonMetaRoot(), "time step", json_string("Time step"));
                        json_object_set_new(
                            visualization->getJsonMetaRoot(),
                            "drawing time",
                            json_string("Drawing time in us"));
                        json_object_set_new(
                            visualization->getJsonMetaRoot(),
                            "simulation time",
                            json_string("Simulation time in us"));
                        json_object_set_new(
                            visualization->getJsonMetaRoot(),
                            "cell count",
                            json_string("Total numbers of cells"));
                        json_object_set_new(
                            visualization->getJsonMetaRoot(),
                            "particle count",
                            json_string("Total numbers of particles"));
                    }
                    CommunicatorSetting communicatorBehaviour = reconnect ? RetryEverySend : ReturnAtError;
                    if(visualization->init(communicatorBehaviour) != 0)
                    {
                        if(rank == 0)
                            log<picLog::INPUT_OUTPUT>("ISAAC Init failed, disable plugin");
                        notifyPeriod = "";
                    }
                    else
                    {
                        const int localNrOfCells
                            = cellDescription->getGridLayout().getDataSpaceWithoutGuarding().productOfComponents();
                        cellCount = localNrOfCells * numProc;
                        particleCount = localNrOfCells * particles::TYPICAL_PARTICLES_PER_CELL
                            * (bmpl::size<VectorAllSpecies>::type::value) * numProc;
                        lastNotify = getTicksUs();
                        if(rank == 0)
                        {
                            log<picLog::INPUT_OUTPUT>("ISAAC Init succeded");
                        }
                    }
                    if(rank == 0)
                    {
                        json_t* feedback = json_object();
                        json_t* array = json_array();
                        json_array_append_new(array, json_real(1.0));
                        json_array_append_new(array, json_real(1.0));
                        json_array_append_new(array, json_real(0.0));
                        json_array_append_new(array, json_real(1.0));
                        json_object_set_new(feedback, "rotation axis", array);
                        visualization->getCommunicator()->setMessage(feedback);

                        if(!timingsFilename.empty())
                        {
                            // Initialization if benchmarking run is started
                            timingsFile.open(timingsFilename, std::ios::out | std::ios::trunc);
                            std::cout << "Benchmark start filename: " << timingsFilename << std::endl;
                            if(timingsFile)
                                std::cout << "File was opened!" << std::endl;
                            else
                                std::cout << "File couldn't be opened!" << std::endl;
                            timingsFile << "Timestep,";
                            timingsFile << "min-sim,max-sim,average-sim,"
                                        << "min-vis,max-vis,average-vis,"
                                        << "min-kernel,max-kernel,average-kernel,"
                                        << "min-merge,max-merge,average-merge,"
                                        << "min-videoSend,max-videoSend,average-videoSend,"
                                        << "min-copy,max-copy,average-copy,"
                                        << "min-sorting,max-sorting,average-sorting,"
                                        << "min-buffer,max-buffer,average-buffer,"
                                        << "min-advection,max-advection,average-advection,"
                                        << "min-advectionBorder,max-advectionBorder,average-advectionBorder,"
                                        << "min-optimizationBuffer,max-optimizationBuffer,average-optimizationBuffer"
                                        << "\n";
                            json_t* feedback = json_object();
                            json_t* weights = json_array();
                            json_array_append_new(weights, json_real(double(7.0)));
                            json_array_append_new(weights, json_real(double(7.0)));
                            json_array_append_new(weights, json_real(double(7.0)));
                            json_array_append_new(weights, json_real(double(0.0)));
                            json_array_append_new(weights, json_real(double(0.0)));
                            json_object_set_new(feedback, "weight", weights);
                            json_t* isoThresholds = json_array();
                            json_array_append_new(isoThresholds, json_real(double(1.0)));
                            json_array_append_new(isoThresholds, json_real(double(1.0)));
                            json_array_append_new(isoThresholds, json_real(double(1.0)));
                            json_object_set_new(feedback, "iso threshold", isoThresholds);
                            json_object_set_new(feedback, "interpolation", json_boolean(true));
                            json_object_set_new(feedback, "distance relative", json_real(2.5));
                            visualization->getCommunicator()->setMessage(feedback);
                        }
                    }
                    if(!timingsFilename.empty())
                        recording = true;
                }
                Environment<>::get().PluginConnector().setNotificationPeriod(this, notifyPeriod);
            }

            void pluginUnload() override
            {
                if(!notifyPeriod.empty())
                {
                    delete visualization;
                    visualization = nullptr;
                    if(rank == 0)
                        log<picLog::INPUT_OUTPUT>("ISAAC finished");
                }
            }
        };

    } // namespace isaacP
} // namespace picongpu
