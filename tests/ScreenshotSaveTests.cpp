// Compiled after the production functions by prepare_screenshot_save_tests.py.
static std::string ReadFile(const std::filesystem::path& path) {
	std::ifstream in(path, std::ios::binary);
	return { std::istreambuf_iterator<char>(in), {} };
}

static void DecodeSize(const std::filesystem::path& path, uint32_t width, uint32_t height) {
	winrt::com_ptr<IWICImagingFactory2> factory;
	assert(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put()))));
	winrt::com_ptr<IWICBitmapDecoder> decoder;
	assert(SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.put())));
	winrt::com_ptr<IWICBitmapFrameDecode> frame;
	assert(SUCCEEDED(decoder->GetFrame(0, frame.put())));
	UINT actualWidth = 0, actualHeight = 0;
	assert(SUCCEEDED(frame->GetSize(&actualWidth, &actualHeight)));
	assert(actualWidth == width && actualHeight == height);
}

int wmain(int argc, wchar_t** argv) {
	assert(argc == 2);
	using namespace Magpie;
	winrt::init_apartment(winrt::apartment_type::multi_threaded);
	const auto root = std::filesystem::path(argv[1]) / (L"disk-" + std::to_wstring(GetCurrentProcessId()));
	std::filesystem::create_directories(root);
	const auto cleanup = wil::scope_exit([&] { std::filesystem::remove_all(root); });
	std::vector<uint8_t> pixels(26 * 14 * 4, 255);
	auto save = [&](const std::filesystem::path& path, uint32_t w = 13, uint32_t h = 7) {
		TextureSaveError error;
		return TextureHelper::SaveTextureExclusive(path.c_str(), w, h,
			DXGI_FORMAT_R8G8B8A8_UNORM, pixels, w * 4, &error);
	};
	assert(ScreenshotHelper::FindUnusedScreenshotNum(root) == 1);
	assert(save(root/L"Magpie_001_input.png"));
	assert(save(root/L"Magpie_001_output.png", 26, 14));
	DecodeSize(root/L"Magpie_001_input.png", 13, 7);
	DecodeSize(root/L"Magpie_001_output.png", 26, 14);
	assert(ScreenshotHelper::FindUnusedScreenshotNum(root) == 2);
	const auto original = ReadFile(root/L"Magpie_001_input.png");
	assert(!original.empty());
	assert(!save(root/L"Magpie_001_input.png"));
	assert(ReadFile(root/L"Magpie_001_input.png") == original);
	std::filesystem::create_directory(root/L"Magpie_002_output.png");
	assert(save(root/L"Magpie_002_input.png"));
	assert(!save(root/L"Magpie_002_output.png"));
	assert(std::filesystem::is_regular_file(root/L"Magpie_002_input.png"));
	assert(ScreenshotHelper::FindUnusedScreenshotNum(root) == 3);
	assert(save(root/L"Magpie_003.png"));
	std::ofstream(root/L"Magpie_004.dds") << "intermediate";
	assert(ScreenshotHelper::FindUnusedScreenshotNum(root) == 5);
	assert(save(root/L"Magpie_4294967295.png"));
	assert(ScreenshotHelper::FindUnusedScreenshotNum(root) == 5);
	std::vector<std::thread> workers;
	for (int i = 0; i < 8; ++i) workers.emplace_back([&] {
		winrt::init_apartment(winrt::apartment_type::multi_threaded);
		std::lock_guard lock(ScreenshotHelper::SaveMutex());
		const uint32_t number = ScreenshotHelper::FindUnusedScreenshotNum(root);
		assert(save(root/fmt::format(L"Magpie_{:03}_input.png", number)));
		assert(save(root/fmt::format(L"Magpie_{:03}_output.png", number), 26, 14));
	});
	for (auto& worker : workers) worker.join();
	assert(ScreenshotHelper::FindUnusedScreenshotNum(root) == 13);
	for (uint32_t i = 5; i < 13; ++i) {
		DecodeSize(root/fmt::format(L"Magpie_{:03}_input.png", i), 13, 7);
		DecodeSize(root/fmt::format(L"Magpie_{:03}_output.png", i), 26, 14);
	}
	for (const auto& entry : std::filesystem::directory_iterator(root))
		assert(!entry.path().filename().native().starts_with(L".Magpie-"));
	TextureSaveError error;
	const auto missing = root/L"missing"/L"Magpie_001.png";
	assert(!TextureHelper::SaveTextureExclusive(missing.c_str(), 1, 1,
		DXGI_FORMAT_R8G8B8A8_UNORM, pixels, 4, &error));
	assert(error.fileWriteFailed);
	std::cout << "PASS: production WIC PNG sizes, pair/legacy/DDS numbering, overflow, eight concurrent pairs, partial failure, no overwrite and temp cleanup\n";
}
