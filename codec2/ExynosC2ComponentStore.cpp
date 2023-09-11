/*
 * Copyright 2018 The Android Open Source Project
 * Copyright 2023 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <util/C2InterfaceHelper.h>

#include "ExynosC2ComponentStore.h"
#include "ExynosIONUtils.h"

class ExynosC2ComponentStore : public C2ComponentStore {
public:
    ExynosC2ComponentStore()
        : mReflectorHelper(std::make_shared<C2ReflectorHelper>()),
          mInterface(mReflectorHelper) {
    }

    virtual ~ExynosC2ComponentStore() override = default;

    virtual C2String getName() const override {
        return "vendor.componentStore.exynos.omx";
    }

    virtual c2_status_t createComponent(
            C2String /*name*/,
            std::shared_ptr<C2Component>* const /*component*/) override {
        return C2_NOT_FOUND;
    }

    virtual c2_status_t createInterface(
            C2String /* name */,
            std::shared_ptr<C2ComponentInterface>* const /* interface */) override {
        return C2_NOT_FOUND;
    }

    virtual std::vector<std::shared_ptr<const C2Component::Traits>>
            listComponents() override {
        return {};
    }

    virtual c2_status_t copyBuffer(
            std::shared_ptr<C2GraphicBuffer> /* src */,
            std::shared_ptr<C2GraphicBuffer> /* dst */) override {
        return C2_OMITTED;
    }

    virtual c2_status_t query_sm(
        const std::vector<C2Param*>& stackParams,
        const std::vector<C2Param::Index>& heapParamIndices,
        std::vector<std::unique_ptr<C2Param>>* const heapParams) const override {
        return mInterface.query(stackParams, heapParamIndices, C2_MAY_BLOCK, heapParams);
    }

    virtual c2_status_t config_sm(
            const std::vector<C2Param*>& params,
            std::vector<std::unique_ptr<C2SettingResult>>* const failures) override {
        return mInterface.config(params, C2_MAY_BLOCK, failures);
    }

    virtual std::shared_ptr<C2ParamReflector> getParamReflector() const override {
        return mReflectorHelper;
    }

    virtual c2_status_t querySupportedParams_nb(
            std::vector<std::shared_ptr<C2ParamDescriptor>>* const params) const override {
        return mInterface.querySupportedParams(params);
    }

    virtual c2_status_t querySupportedValues_sm(
            std::vector<C2FieldSupportedValuesQuery>& fields) const override {
        return mInterface.querySupportedValues(fields, C2_MAY_BLOCK);
    }

private:
    class Interface : public C2InterfaceHelper {
    public:
        Interface(const std::shared_ptr<C2ReflectorHelper> &helper)
            : C2InterfaceHelper(helper) {
            setDerivedInstance(this);

            addParameter(
                DefineParam(mIonUsageInfo, "ion-usage")
                .withDefault(new C2StoreIonUsageInfo())
                .withFields({
                    C2F(mIonUsageInfo, usage).flags(
                            {C2MemoryUsage::CPU_READ | C2MemoryUsage::CPU_WRITE}),
                    C2F(mIonUsageInfo, capacity).inRange(0, UINT32_MAX, 1024),
                    C2F(mIonUsageInfo, heapMask).any(),
                    C2F(mIonUsageInfo, allocFlags).flags({}),
                    C2F(mIonUsageInfo, minAlignment).equalTo(0)
                })
                .withSetter(SetIonUsage)
                .build());

            addParameter(
                DefineParam(mDmaBufUsageInfo, "dmabuf-usage")
                .withDefault(C2StoreDmaBufUsageInfo::AllocShared(ExynosIONUtils::MAX_HEAP_NAME, 0, 0))
                .withFields({
                    C2F(mDmaBufUsageInfo, m.usage).flags({C2MemoryUsage::CPU_READ | C2MemoryUsage::CPU_WRITE}),
                    C2F(mDmaBufUsageInfo, m.capacity).inRange(0, UINT32_MAX, 1024),
                    C2F(mDmaBufUsageInfo, m.allocFlags).flags({ (int)ExynosIONUtils::getDmaUsageMask() }),
                    C2F(mDmaBufUsageInfo, m.heapName).any(),
                })
                .withSetter(setDmaBufUsage)
                .build());
        }

        virtual ~Interface() = default;

    private:
        static C2R SetIonUsage(bool /* mayBlock */, C2P<C2StoreIonUsageInfo> &me) {
            return ExynosIONUtils::setIonUsage(me);
        }

        static C2R setDmaBufUsage(bool /* mayBlock */, C2P<C2StoreDmaBufUsageInfo> &me) {
            return ExynosIONUtils::setDmaUsage(me);
        }

        std::shared_ptr<C2StoreIonUsageInfo> mIonUsageInfo;
        std::shared_ptr<C2StoreDmaBufUsageInfo> mDmaBufUsageInfo;
    };
    std::shared_ptr<C2ReflectorHelper> mReflectorHelper;
    Interface mInterface;
};

std::shared_ptr<C2ComponentStore> GetCodec2ExynosComponentStore() {
    static std::mutex mutex;

    static std::weak_ptr<C2ComponentStore> C2Store;

    std::lock_guard<std::mutex> lock(mutex);

    std::shared_ptr<C2ComponentStore> store = nullptr;

    if (!C2Store.expired()) {
        store = C2Store.lock();
    }

    if (store.get() == nullptr) {
        store = std::make_shared<ExynosC2ComponentStore>();
        C2Store = store;
    }

    return store;
}
