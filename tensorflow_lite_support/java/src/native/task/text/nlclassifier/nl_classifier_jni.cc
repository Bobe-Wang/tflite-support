/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <jni.h>

#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/op_resolver.h"
#include "tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"

namespace tflite {
namespace task {
// To be provided by a link-time library
extern std::unique_ptr<OpResolver> CreateOpResolver();

}  // namespace task
}  // namespace tflite

namespace {

using ::tflite::support::task::core::Category;
using ::tflite::support::task::text::nlclassifier::NLClassifier;
using ::tflite::support::task::text::nlclassifier::NLClassifierOptions;
using ::tflite::support::utils::ConvertVectorToArrayList;
using ::tflite::support::utils::GetMappedFileBuffer;
using ::tflite::support::utils::JStringToString;

constexpr int kInvalidPointer = 0;

NLClassifierOptions ConvertJavaNLClassifierOptions(
    JNIEnv* env, jobject java_nl_classifier_options) {
  jclass nl_classifier_options_class = env->FindClass(
      "org/tensorflow/lite/task/text/nlclassifier/"
      "NLClassifier$NLClassifierOptions");
  jmethodID input_tensor_index_method_id =
      env->GetMethodID(nl_classifier_options_class, "inputTensorIndex", "()I");
  jmethodID output_score_tensor_index_method_id = env->GetMethodID(
      nl_classifier_options_class, "outputScoreTensorIndex", "()I");
  jmethodID output_label_tensor_index_method_id = env->GetMethodID(
      nl_classifier_options_class, "outputLabelTensorIndex", "()I");
  jmethodID input_tensor_name_method_id = env->GetMethodID(
      nl_classifier_options_class, "inputTensorName", "()Ljava/lang/String;");
  jmethodID output_score_tensor_name_method_id =
      env->GetMethodID(nl_classifier_options_class, "outputScoreTensorName",
                       "()Ljava/lang/String;");
  jmethodID output_label_tensor_name_method_id =
      env->GetMethodID(nl_classifier_options_class, "outputLabelTensorName",
                       "()Ljava/lang/String;");

  return {
      .input_tensor_index = env->CallIntMethod(java_nl_classifier_options,
                                               input_tensor_index_method_id),
      .output_score_tensor_index = env->CallIntMethod(
          java_nl_classifier_options, output_score_tensor_index_method_id),
      .output_label_tensor_index = env->CallIntMethod(
          java_nl_classifier_options, output_label_tensor_index_method_id),
      .input_tensor_name = JStringToString(
          env, (jstring)env->CallObjectMethod(java_nl_classifier_options,
                                              input_tensor_name_method_id)),
      .output_score_tensor_name = JStringToString(
          env,
          (jstring)env->CallObjectMethod(java_nl_classifier_options,
                                         output_score_tensor_name_method_id)),
      .output_label_tensor_name = JStringToString(
          env,
          (jstring)env->CallObjectMethod(java_nl_classifier_options,
                                         output_label_tensor_name_method_id)),
  };
}

extern "C" JNIEXPORT void JNICALL
Java_org_tensorflow_lite_task_core_BaseTaskApi_deinitJni(JNIEnv* env,
                                                         jobject thiz,
                                                         jlong native_handle) {
  delete reinterpret_cast<NLClassifier*>(native_handle);
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_text_nlclassifier_NLClassifier_initJniWithByteBuffer(
    JNIEnv* env, jclass thiz, jobject nl_classifier_options,
    jobject model_buffer) {
  auto model = GetMappedFileBuffer(env, model_buffer);
  absl::StatusOr<std::unique_ptr<NLClassifier>> status =
      NLClassifier::CreateNLClassifier(
          model.data(), model.size(),
          ConvertJavaNLClassifierOptions(env, nl_classifier_options),
          tflite::task::CreateOpResolver());

  if (status.ok()) {
    return reinterpret_cast<jlong>(status->release());
  } else {
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jobject JNICALL
Java_org_tensorflow_lite_task_text_nlclassifier_NLClassifier_classifyNative(
    JNIEnv* env, jclass thiz, jlong native_handle, jstring text) {
  auto* question_answerer = reinterpret_cast<NLClassifier*>(native_handle);

  auto results = question_answerer->Classify(JStringToString(env, text));
  jclass category_class =
      env->FindClass("org/tensorflow/lite/support/label/Category");
  jmethodID category_init =
      env->GetMethodID(category_class, "<init>", "(Ljava/lang/String;F)V");

  return ConvertVectorToArrayList<Category>(
      env, results,
      [env, category_class, category_init](const Category& category) {
        jstring class_name = env->NewStringUTF(category.class_name.data());
        // Convert double to float as Java interface exposes float as scores.
        jobject jcategory = env->NewObject(category_class, category_init,
                                           class_name, (float)category.score);
        env->DeleteLocalRef(class_name);
        return jcategory;
      });
}

}  // namespace
