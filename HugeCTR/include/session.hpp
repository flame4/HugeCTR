/*
 * Copyright (c) 2020, NVIDIA CORPORATION.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <thread>
#include <utility>
#include "HugeCTR/include/common.hpp"
#include "HugeCTR/include/data_reader_worker.hpp"
#include "HugeCTR/include/device_map.hpp"
#include "HugeCTR/include/embedding.hpp"
#include "HugeCTR/include/metrics.hpp"
#include "HugeCTR/include/network.hpp"
#include "HugeCTR/include/parser.hpp"
#include "ctpl/ctpl_stl.h"

namespace HugeCTR {

class Session {
 public:
  static std::shared_ptr<Session> Create(const SolverParser& solver_config);

  virtual void train() = 0;
  virtual void eval() = 0;
  virtual std::vector<std::pair<std::string, float>> get_eval_metrics() = 0;
  virtual void start_data_reading() = 0;
  virtual Error_t get_current_loss(float* loss) = 0;
  virtual Error_t download_params_to_files(std::string prefix, int iter) = 0;
  virtual Error_t set_learning_rate(float lr) = 0;
  virtual void check_overflow() const = 0;
};

/**
 * @brief A simple facade of HugeCTR.
 *
 * This is a class supporting basic usages of hugectr, which includes
 * train; evaluation; get loss; load and download trained parameters.
 * To learn how to use those method, please refer to main.cpp.
 */
template <typename TypeKey>
class SessionImpl : public Session {
 public:
  /**
   * Dtor of SessionImpl.
   */
  ~SessionImpl();
  SessionImpl(const SessionImpl&) = delete;
  SessionImpl& operator=(const SessionImpl&) = delete;

  /**
   * The all in one training method.
   * This method processes one iteration of a training, including one forward, one backward and
   * parameter update
   */
  void train() override;
  /**
   * The all in one evaluation method.
   * This method processes one forward of evaluation.
   */
  void eval() override;

  std::vector<std::pair<std::string, float>> get_eval_metrics() override;

  void start_data_reading() override {
    data_reader_->start();
    data_reader_eval_->start();
  }

  /**
   * Get current loss from the loss tensor.
   * @return loss in float
   */
  Error_t get_current_loss(float* loss) override;
  /**
   * Download trained parameters to file.
   * @param weights_file file name of output dense model
   * @param embedding_file file name of output sparse model
   */
  Error_t download_params_to_files(std::string prefix, int iter) override;

  /**
   * Set learning rate while training
   * @param lr learning rate.
   */
  Error_t set_learning_rate(float lr) override {
    for (auto& embedding : embedding_) {
      embedding->set_learning_rate(lr);
    }
    for (auto& network : networks_) {
      network->set_learning_rate(lr);
    }
    return Error_t::Success;
  }
  /**
   * generate a dense model and initilize with small random values.
   * @param model_file dense model initilized
   */
  Error_t init_params(std::string model_file);
  /**
   * get the number of parameters (reserved for debug)
   */
  long long get_params_num() {
    long long size = 0;
    for (auto& embedding : embedding_) {
      size += embedding->get_params_num();
    }
    return static_cast<long long>(networks_[0]->get_params_num()) + size;
  }

  void check_overflow() const override;

 private:
  // typedef unsigned int TypeKey; /**< type of input key in dataset. */
  /// typedef long long TypeKey;                        /**< type of input key in dataset. */
  std::vector<std::unique_ptr<Network>> networks_;      /**< networks (dense) used in training. */
  std::vector<std::unique_ptr<IEmbedding>> embedding_;  /**< embedding */
  std::vector<std::unique_ptr<Network>> networks_eval_; /**< networks (dense) used in eval. */
  std::vector<std::unique_ptr<IEmbedding>> embedding_eval_; /**< embedding in eval*/

  std::unique_ptr<DataReader<TypeKey>>
      data_reader_; /**< data reader to reading data from data set to embedding. */
  std::unique_ptr<DataReader<TypeKey>> data_reader_eval_; /**< data reader for evaluation. */
  std::shared_ptr<GPUResourceGroup>
      gpu_resource_group_; /**< GPU resources include handles and streams etc.*/

  Error_t download_params_to_files_(std::string weights_file,
                                    const std::vector<std::string>& embedding_files);

  metrics::Metrics metrics_;

  friend std::shared_ptr<Session> Session::Create(const SolverParser& solver_config);
  SessionImpl(const SolverParser& solver_config);

  /**
   * A method load trained parameters for dense model.
   * @param model_file dense model generated by training
   */
  Error_t load_params_for_dense_(const std::string& model_file);

  /**
   * A method initialize or load trained parameters for sparse model.
   * @param embedding_model_file sparse model generated by training
   */
  Error_t init_or_load_params_for_sparse_(const std::vector<std::string>& embedding_file);
};

}  // namespace HugeCTR
