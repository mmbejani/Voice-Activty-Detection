#include "dataset.hh"

DEFINE_uint32(batch_size, 32, "Size of batch size");
DEFINE_string(manifest_path, "/home/mahdi/data/with-noise.json", "Path to tsv (TAB Seperated Values) meta data for training");

namespace za{

    VADDataset::VADDataset(std::string &manifest_path, uint32_t batch_size){
        this->audioLoader = [](const std::string &audioPath){
                   std::vector<float>data = fl::pkg::speech::loadSound<float>(audioPath);
                   af::array af_data(data.size(), af::dtype::f32);
                   af_data.write(data.data(), data.size() * sizeof(float));
                   return af_data;
                };

        this->audioCollator = [](const std::vector<af::array> &dataList){
            unsigned int max_len_data = 0;
            const int cat_dim = 1;
            const af::dim4 zero_dim(0,0,0,0);
            for (size_t i = 0; i < dataList.size(); i++)
                if (max_len_data < dataList[i].dims(0))
                    max_len_data = dataList[i].dims(0);

            std::vector<af::array> pad_arrays;
            for (size_t i = 0; i < dataList.size(); i++){
                auto array = dataList[i];
                af::dim4 pad_size(max_len_data - array.dims(0), 0, 0, 0);
                auto pad_array = af::pad(dataList[i], 
                                         pad_size, 
                                         zero_dim,
                                         af::borderType::AF_PAD_ZERO);
                pad_arrays.push_back(pad_array);
            }

            af::array pad_array = cat_array(cat_dim, pad_arrays);
            return pad_array;
        };

        LOG(INFO) << "Reading Manifest From " << FLAGS_manifest_path << " ...";
        try{
            std::unique_ptr<std::ifstream> reader = 
                std::make_unique<std::ifstream>(FLAGS_manifest_path);
            Json::Value root;
            Json::CharReaderBuilder builder;

            builder["collectComments"] = true;
            JSONCPP_STRING errs;
            Json::parseFromStream(builder, *reader, &root, &errs);
            for (unsigned int i = 0; i < root.size(); i++)
            {
                this->audio_paths.push_back(root[i]["audio_path"].asString());
                this->audio_labels.push_back(root[i]["audio_label"].asInt());
            }

            reader->close();
        }catch(...){
            LOG(INFO) << "Manifest file is not readable ...";
            exit(-1);
        }
    }

    int64_t VADDataset::size() const { return this->audio_labels.size(); }

    std::vector<af::array> VADDataset::get(const int64_t idx) const {
        constexpr float one[1] = {1};
        constexpr float zero[1] = {1};
        return {this->audioLoader(this->audio_paths[idx]),
                this->audio_labels[idx] == 1? af::array(1, one): af::array(1, zero)};
    }

    std::unique_ptr<VADDataset> getDataset(){
        return std::make_unique<VADDataset>();
    }

    af::array cat_array(const int dim, const std::vector<af::array>& arrays){
        if (arrays.size() == 1) [[unlikely]]
            return arrays[0];

        auto cat = af::join(dim, arrays[0], arrays[1]);
        for (unsigned int i = 2; i < arrays.size(); i++)
            cat = af::join(dim, cat, arrays[i]);
        
        [[likely]]return cat;
    }
}