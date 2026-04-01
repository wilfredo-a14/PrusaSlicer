///|/ Copyright (c) Prusa Research 2022 - 2023 Tomáš Mészáros @tamasmeszaros
///|/ Copyright (c) 2023 Mimoja @Mimoja
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include "SLAArchiveWriter.hpp"

#include "SLAArchiveFormatRegistry.hpp"
#include "libslic3r/PrintConfig.hpp"

#include <filesystem>
#include <fstream>
#include <boost/log/trivial.hpp>
#include "libslic3r/format.hpp"

namespace Slic3r {

std::unique_ptr<SLAArchiveWriter>
SLAArchiveWriter::create(const std::string &archtype, const SLAPrinterConfig &cfg)
{
    std::unique_ptr<SLAArchiveWriter> ret;
    auto factory = get_writer_factory(archtype.c_str());

    if (factory)
        ret = factory(cfg);

    return ret;
}

// Export all rasterized layers as PNG images to output directory
void SLAArchiveWriter::export_layers_as_png(const std::string &output_dir) const
{
    namespace fs = std::filesystem;

    try {
        // Create output directory if it doesn't exist
        fs::create_directories(output_dir);

        // Export each layer as PNG using the PNG-encoded versions
        if (m_png_layers.empty()) {
            BOOST_LOG_TRIVIAL(warning) << "No PNG layers available for export";
            return;
        }

        for (size_t layer_id = 0; layer_id < m_png_layers.size(); ++layer_id) {
            // Generate filename: layer_XXXX.png (4-digit zero-padded layer index)
            // Using printf-style formatting: %04d = zero-padded 4-digit integer
            std::string filename = Slic3r::format("layer_%04d.png", layer_id);
            fs::path filepath = fs::path(output_dir) / filename;

            // Open file for binary writing
            std::ofstream outfile(filepath, std::ios::binary);
            if (!outfile) {
                BOOST_LOG_TRIVIAL(warning) << "Failed to open PNG export file: " << filepath.string();
                continue;
            }

            // Write PNG data to file
            const sla::EncodedRaster& encoded = m_png_layers[layer_id];
            outfile.write(static_cast<const char*>(encoded.data()), encoded.size());
            outfile.close();

            BOOST_LOG_TRIVIAL(debug) << "Exported PNG layer: " << filepath.string();
        }

        BOOST_LOG_TRIVIAL(info) << "Successfully exported " << m_png_layers.size() << " PNG layers to: " << output_dir;

    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "PNG export failed: " << e.what();
    }
}

} // namespace Slic3r
