#include "zoneout/zoneout/plot.hpp"

namespace zoneout {

    Plot::Plot(const std::string &name, const std::string &type, const concord::Datum &datum)
        : id_(generateUUID()), name_(name), type_(type), datum_(datum) {}

    Plot::Plot(const UUID &id, const std::string &name, const std::string &type, const concord::Datum &datum)
        : id_(id), name_(name), type_(type), datum_(datum) {}

    const UUID &Plot::get_id() const { return id_; }

    const std::string &Plot::get_name() const { return name_; }
    void Plot::set_name(const std::string &name) { name_ = name; }

    const std::string &Plot::get_type() const { return type_; }
    void Plot::set_type(const std::string &type) { type_ = type; }

    const concord::Datum &Plot::get_datum() const { return datum_; }
    void Plot::set_datum(const concord::Datum &datum) { datum_ = datum; }

    void Plot::add_zone(const Zone &zone) { zones_.push_back(zone); }

    bool Plot::remove_zone(const UUID &zone_id) {
        auto it = std::find_if(zones_.begin(), zones_.end(),
                               [&zone_id](const Zone &zone) { return zone.id() == zone_id; });
        if (it != zones_.end()) {
            zones_.erase(it);
            return true;
        }
        return false;
    }

    Zone *Plot::get_zone(const UUID &zone_id) {
        auto it = std::find_if(zones_.begin(), zones_.end(),
                               [&zone_id](const Zone &zone) { return zone.id() == zone_id; });
        return (it != zones_.end()) ? &(*it) : nullptr;
    }

    const Zone *Plot::get_zone(const UUID &zone_id) const {
        auto it = std::find_if(zones_.begin(), zones_.end(),
                               [&zone_id](const Zone &zone) { return zone.id() == zone_id; });
        return (it != zones_.end()) ? &(*it) : nullptr;
    }

    const std::vector<Zone> &Plot::get_zones() const { return zones_; }
    std::vector<Zone> &Plot::get_zones() { return zones_; }

    size_t Plot::get_zone_count() const { return zones_.size(); }

    bool Plot::empty() const { return zones_.empty(); }

    void Plot::clear() { zones_.clear(); }

    void Plot::set_property(const std::string &key, const std::string &value) { properties_[key] = value; }

    std::string Plot::get_property(const std::string &key) const {
        auto it = properties_.find(key);
        return (it != properties_.end()) ? it->second : "";
    }

    const std::unordered_map<std::string, std::string> &Plot::get_properties() const { return properties_; }

    bool Plot::is_valid() const { return !name_.empty() && !type_.empty(); }

    void Plot::save(const std::filesystem::path &directory) const {
        std::filesystem::create_directories(directory);

        for (size_t i = 0; i < zones_.size(); ++i) {
            auto zone_dir = directory / ("zone_" + std::to_string(i));
            std::filesystem::create_directories(zone_dir);
            auto vector_path = zone_dir / "vector.geojson";
            auto raster_path = zone_dir / "raster.tiff";
            zones_[i].to_files(vector_path, raster_path);
        }
    }

    void Plot::save_tar(const std::filesystem::path &tar_file) const {
        mtar_t tar;
        int err = mtar_open(&tar, tar_file.string().c_str(), "w");
        if (err != MTAR_ESUCCESS) {
            throw std::runtime_error("Could not create tar file: " + std::string(mtar_strerror(err)));
        }

        auto temp_dir = std::filesystem::temp_directory_path() / ("plot_" + id_.toString());
        save(temp_dir);

        for (const auto &entry : std::filesystem::recursive_directory_iterator(temp_dir)) {
            if (entry.is_regular_file()) {
                auto relative_path = std::filesystem::relative(entry.path(), temp_dir);
                std::ifstream file(entry.path(), std::ios::binary);

                if (file.is_open()) {
                    file.seekg(0, std::ios::end);
                    size_t file_size = file.tellg();
                    file.seekg(0, std::ios::beg);

                    err = mtar_write_file_header(&tar, relative_path.string().c_str(), file_size);
                    if (err != MTAR_ESUCCESS) {
                        file.close();
                        mtar_close(&tar);
                        std::filesystem::remove_all(temp_dir);
                        throw std::runtime_error("Could not write file header: " + std::string(mtar_strerror(err)));
                    }

                    const size_t chunk_size = 8192;
                    std::vector<char> buffer(chunk_size);
                    while (file.read(buffer.data(), chunk_size) || file.gcount() > 0) {
                        err = mtar_write_data(&tar, buffer.data(), file.gcount());
                        if (err != MTAR_ESUCCESS) {
                            file.close();
                            mtar_close(&tar);
                            std::filesystem::remove_all(temp_dir);
                            throw std::runtime_error("Could not write file data: " + std::string(mtar_strerror(err)));
                        }
                    }
                    file.close();
                }
            }
        }

        mtar_finalize(&tar);
        mtar_close(&tar);
        std::filesystem::remove_all(temp_dir);
    }

    void Plot::to_files(const std::filesystem::path &directory) const { save(directory); }

    Plot Plot::load_tar(const std::filesystem::path &tar_file, const std::string &name, const std::string &type,
                        const concord::Datum &datum) {
        mtar_t tar;
        int err = mtar_open(&tar, tar_file.string().c_str(), "r");
        if (err != MTAR_ESUCCESS) {
            throw std::runtime_error("Could not open tar file: " + std::string(mtar_strerror(err)));
        }

        auto temp_dir = std::filesystem::temp_directory_path() / ("extract_" + std::to_string(std::time(nullptr)));
        std::filesystem::create_directories(temp_dir);

        mtar_header_t header;
        while ((err = mtar_read_header(&tar, &header)) == MTAR_ESUCCESS) {
            auto file_path = temp_dir / header.name;
            std::filesystem::create_directories(file_path.parent_path());

            std::ofstream file(file_path, std::ios::binary);
            if (file.is_open()) {
                const size_t chunk_size = 8192;
                std::vector<char> buffer(chunk_size);
                size_t remaining = header.size;

                while (remaining > 0) {
                    size_t to_read = std::min(chunk_size, remaining);
                    err = mtar_read_data(&tar, buffer.data(), to_read);
                    if (err != MTAR_ESUCCESS) {
                        file.close();
                        mtar_close(&tar);
                        std::filesystem::remove_all(temp_dir);
                        throw std::runtime_error("Could not read file data: " + std::string(mtar_strerror(err)));
                    }
                    file.write(buffer.data(), to_read);
                    remaining -= to_read;
                }
                file.close();
            }

            err = mtar_next(&tar);
            if (err != MTAR_ESUCCESS && err != MTAR_ENULLRECORD) {
                mtar_close(&tar);
                std::filesystem::remove_all(temp_dir);
                throw std::runtime_error("Could not advance to next file: " + std::string(mtar_strerror(err)));
            }
        }

        mtar_close(&tar);

        Plot plot = load(temp_dir, name, type, datum);

        std::filesystem::remove_all(temp_dir);

        return plot;
    }

    Plot Plot::load(const std::filesystem::path &directory, const std::string &name, const std::string &type,
                    const concord::Datum &datum) {
        Plot plot(name, type, datum);
        concord::Datum plot_datum;

        if (std::filesystem::exists(directory)) {
            for (const auto &entry : std::filesystem::directory_iterator(directory)) {
                if (entry.is_directory() && entry.path().filename().string().starts_with("zone_")) {
                    try {
                        auto vector_path = entry.path() / "vector.geojson";
                        auto raster_path = entry.path() / "raster.tiff";
                        auto zone = Zone::from_files(vector_path, raster_path);
                        plot_datum = zone.datum();
                        plot.add_zone(zone);
                    } catch (const std::exception &) {
                    }
                }
            }
        }
        plot.datum_ = plot_datum;
        return plot;
    }

    Plot Plot::from_files(const std::filesystem::path &directory, const std::string &name, const std::string &type,
                         const concord::Datum &datum) {
        return load(directory, name, type, datum);
    }

} // namespace zoneout
