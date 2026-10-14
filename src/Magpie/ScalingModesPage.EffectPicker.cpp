#include "pch.h"
#include "ScalingModesPage.h"
#include "EffectCatalog.h"
#include "EffectHelper.h"
#include "EffectsService.h"
#include "EffectPickerLayout.h"
#include "App.h"
#include "CommonSharedConstants.h"
#include "MainWindow.h"
#include "XamlHelper.h"
#include <shellscalingapi.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Input.h>
using namespace ::Magpie;
using namespace winrt;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Input;
using Windows::UI::Xaml::Automation::AutomationProperties;
namespace winrt::Magpie::implementation {
namespace {
std::wstring PickerString(std::wstring_view key) {
	return std::wstring(ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID)
		.GetString(hstring(key)));
}
std::wstring PickerEffectCount(std::wstring_view name, size_t count) {
	return std::wstring(name) + PickerString(L"EffectPicker_NameSeparator") +
		std::to_wstring(count) + PickerString(L"EffectPicker_EffectCountSuffix");
}
TextBlock PickerText(std::wstring_view text, double size = 13, bool singleLine = false) {
	TextBlock block;
	block.Text(text);
	block.FontSize(size);
	block.TextWrapping(singleLine ? TextWrapping::NoWrap : TextWrapping::Wrap);
	if (singleLine) {
		block.MaxLines(1);
		block.TextTrimming(TextTrimming::CharacterEllipsis);
	}
	return block;
}
ScrollViewer PickerScroll(UIElement const &content) {
	ScrollViewer scroll;
	scroll.Content(content);
	scroll.HorizontalScrollMode(ScrollMode::Disabled);
	scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
	scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
	scroll.ZoomMode(ZoomMode::Disabled);
	scroll.IsTabStop(false);
	return scroll;
}
void Row(Grid const &grid, double height, GridUnitType type) {
	RowDefinition row;
	row.Height({height, type});
	grid.RowDefinitions().Append(row);
}
void Column(Grid const &grid, double width, GridUnitType type) {
	ColumnDefinition column;
	column.Width({width, type});
	grid.ColumnDefinitions().Append(column);
}
} // namespace
Button ScalingModesPage::_EffectPickerButton(UIElement const& content) {
	Button button;
	button.Style(Resources().Lookup(box_value(L"EffectPickerButtonStyle")).as<Windows::UI::Xaml::Style>());
	button.Content(content);
	return button;
}
void ScalingModesPage::_SizeEffectPicker(Button const& anchor) {
	const auto& window = App::Get().MainWindow();
	const auto center = anchor.TransformToVisual(XamlRoot().Content()).TransformPoint(
		{float(anchor.ActualWidth() / 2), float(anchor.ActualHeight() / 2)});
	const auto monitor = MonitorFromPoint(window.XamlRootPointToScreen(center), MONITOR_DEFAULTTONEAREST);
	MONITORINFO info{sizeof(info)};
	check_bool(GetMonitorInfo(monitor, &info));
	UINT dpiX = window.CurrentDpi(), dpiY = dpiX;
	GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
	// Windowed XAML popups still use their owning XamlRoot's rasterization scale.
	// The greater scale also bounds the first show across a mixed-DPI seam.
	const double scale = std::max(dpiX / 96.0, XamlRoot().RasterizationScale());
	const auto size = EffectPickerSize(info.rcWork.right - info.rcWork.left,
		info.rcWork.bottom - info.rcWork.top, scale);
	_pickerRoot.Width(size.Width); _pickerRoot.Height(size.Height);
	_pickerRoot.ColumnDefinitions().GetAt(0).Width({std::min(size.Width * 0.4,
		size.Width < 560 ? 160.0 : 220.0), GridUnitType::Pixel});
}
void ScalingModesPage::_BuildEffectPicker() {
	_pickerRows.clear();
	_pickerEntries.clear();
	_pickerCategories.clear();
	_pickerLetters.clear();
	const auto weak = get_weak();
	const auto &catalog = EffectCatalog::Get();
	for (const auto &effect : EffectsService::Get().Effects()) {
		if (RTXVideoStrength(effect.name) >= 0 || effect.name == L"RTXVideo\\RTXVideo_Denoise")
			continue;
		EffectPickerEntry entry;
		if (const auto metadata = catalog.Find(effect.name))
			entry = *metadata;
		else {
			entry.id = effect.name;
			entry.name = EffectHelper::GetDisplayName(effect.name);
			entry.category = L"custom";
			entry.summary = PickerString(L"EffectPicker_CustomSummary");
			entry.details = PickerString(L"EffectPicker_CustomDetails");
			entry.searchText = NormalizeEffectSearch(effect.name);
		}
		_pickerEntries.push_back(std::move(entry));
	}
	auto layout = MakeEffectPickerLayout();
	_pickerRoot = layout.root;
	_pickerCategoryPane = layout.categories;
	_pickerListPane = layout.list;
	_pickerDetailPane = layout.details;
	_pickerDetailContent = StackPanel();
	_pickerDetailContent.Spacing(6);
	_pickerDetailTitle = PickerText(PickerString(L"EffectPicker_All"), 15);
	_pickerDetailTitle.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
	_pickerDetails = PickerText(PickerString(L"EffectPicker_AllDescription"), 12);
	_pickerDetails.IsTextSelectionEnabled(true);
	_pickerDetailContent.Children().Append(_pickerDetailTitle);
	_pickerDetailContent.Children().Append(_pickerDetails);
	_pickerDetailScroll = PickerScroll(_pickerDetailContent);
	_pickerDetailArea = Grid();
	Row(_pickerDetailArea, 1, GridUnitType::Star);
	Row(_pickerDetailArea, 1, GridUnitType::Auto);
	_pickerDetailArea.Children().Append(_pickerDetailScroll);
	_pickerDetailHint = PickerText(PickerString(L"EffectPicker_DetailScrollHint"), 10, true);
	_pickerDetailHint.HorizontalAlignment(HorizontalAlignment::Right);
	_pickerDetailHint.Margin({0, 4, 0, 0});
	_pickerDetailHint.Opacity(0.7);
	_pickerDetailHint.IsHitTestVisible(false);
	_pickerDetailHint.Visibility(Visibility::Collapsed);
	Grid::SetRow(_pickerDetailHint, 1);
	_pickerDetailArea.Children().Append(_pickerDetailHint);
	_pickerDetailPane.Child(_pickerDetailArea);
	for (const FrameworkElement element : {FrameworkElement(_pickerDetailArea), FrameworkElement(_pickerDetailContent)}) {
		element.SizeChanged([weak](auto const&, auto const&) {
			if (auto page = weak.get()) page->_QueueEffectPickerDetailsHint();
		});
	}
	// Receive the wheel even when a nested ScrollViewer has handled its normal
	// input. Ctrl is reserved for details; all picker ScrollViewers disable zoom.
	_pickerRoot.AddHandler(UIElement::PointerWheelChangedEvent(),
		box_value(PointerEventHandler{get_weak(), &ScalingModesPage::_EffectPickerPointerWheelChanged}), true);
	StackPanel categories;
	// Spacing belongs to visible row containers, including collapsed categories.
	auto addCategory = [&](std::wstring name, std::wstring category, std::wstring subcategory,
						   std::wstring description, int parent, bool hasChildren) {
		const size_t index = _pickerCategories.size();
		PickerCategoryRow row;
		row.name = name;
		row.category = category;
		row.subcategory = subcategory;
		row.description = description;
		row.parent = parent;
		row.hasChildren = hasChildren;
		row.button = _EffectPickerButton(nullptr);
		row.button.Padding({0, 8, 6, 8});
		Grid layout;
		Column(layout, hasChildren ? 28 : 16, GridUnitType::Pixel);
		Column(layout, 1, GridUnitType::Star);

		if (hasChildren) {
			row.icon = MakeEffectPickerToggleIcon(row.button);
			layout.Children().Append(row.icon.root);
		}
		Grid label;
		Column(label, 1, GridUnitType::Star);
		Column(label, 1, GridUnitType::Auto);
		label.Children().Append(PickerText(name, 12, true));
		const auto count = std::ranges::count_if(_pickerEntries, [&](const auto &effect) {
			return MatchesEffectCategory(effect, category, subcategory);
		});
		auto number = PickerText(std::to_wstring(count), 10);
		number.Opacity(0.65);
		number.Margin({5, 0, 0, 0});
		number.VerticalAlignment(VerticalAlignment::Center);
		Grid::SetColumn(number, 1);
		label.Children().Append(number);
		label.Margin({4, 0, 0, 0});
		Grid::SetColumn(label, 1);
		layout.Children().Append(label);
		// One button owns the icon, label, count and padding, including all
		// pointer, keyboard and accessibility activation feedback.
		row.button.Content(layout);
		AutomationProperties::SetName(row.button, PickerEffectCount(name, count));
		AutomationProperties::SetHelpText(row.button, description);
		row.button.Click([weak, index](auto const &, auto const &) {
			if (auto page = weak.get()) {
				const auto &item = page->_pickerCategories[index];
				page->_SetEffectCategoryExpanded(index, !item.expanded);
				page->_ChooseEffectCategory(item.category, item.subcategory, item.description);
			}
		});
		auto show = [weak, index](auto const &, auto const &) {
			if (auto page = weak.get()) {
				const auto &item = page->_pickerCategories[index];
				page->_SetEffectPickerDetails(item.name, item.description);
			}
		};
		// Physical pointer movement avoids reselecting a row moved under a stationary cursor.
		row.button.PointerMoved(show);
		row.button.GotFocus(show);
		row.button.KeyDown([weak, index](auto const &, KeyRoutedEventArgs const &args) {
			const auto page = weak.get();
			if (!page)
				return;
			auto &rows = page->_pickerCategories;
			auto &item = rows[index];
			const auto key = args.Key();
			if (key == VirtualKey::Right && item.hasChildren) {
				if (!item.expanded)
					page->_SetEffectCategoryExpanded(index, true);
				else if (index + 1 < rows.size())
					rows[index + 1].button.Focus(FocusState::Keyboard);
				args.Handled(true);
			} else if (key == VirtualKey::Left) {
				if (item.hasChildren && item.expanded)
					page->_SetEffectCategoryExpanded(index, false);
				else if (item.parent >= 0)
					rows[item.parent].button.Focus(FocusState::Keyboard);
				args.Handled(true);
			} else if (key == VirtualKey::Down || key == VirtualKey::Up) {
				const int step = key == VirtualKey::Down ? 1 : -1;
				for (int i = int(index) + step; i >= 0 && i < int(rows.size()); i += step)
					if (rows[i].container.Visibility() == Visibility::Visible) {
						rows[i].button.Focus(FocusState::Keyboard);
						break;
					}
				args.Handled(true);
			}
		});
		Grid rowLayout;
		rowLayout.Children().Append(row.button);
		row.container = Border();
		row.container.Style(Resources().Lookup(box_value(L"EffectPickerCategoryStyle")).as<Windows::UI::Xaml::Style>());
		SetEffectPickerRowMargin(row.container);
		row.selectionMark = Resources().Lookup(box_value(L"EffectPickerSelectionMark")).as<DataTemplate>().LoadContent().as<FrameworkElement>();
		row.selectionMark.IsHitTestVisible(false);
		rowLayout.Children().Append(row.selectionMark);
		row.container.Child(rowLayout);
		if (parent >= 0) {
			SetEffectPickerRowMargin(row.container, 13);
			row.container.BorderThickness({1, 0, 0, 0});
			row.container.Visibility(Visibility::Collapsed);
		}
		categories.Children().Append(row.container);
		_pickerCategories.push_back(std::move(row));
	};
	addCategory(PickerString(L"EffectPicker_GettingStarted"), L"first_try", L"",
		PickerString(L"EffectPicker_GettingStartedDescription"), -1,
				false);
	addCategory(PickerString(L"EffectPicker_Advanced"), L"advanced", L"",
		PickerString(L"EffectPicker_AdvancedDescription"), -1, false);
	addCategory(PickerString(L"EffectPicker_All"), L"", L"",
		PickerString(L"EffectPicker_AllDescription"), -1, false);
	for (const auto &category : catalog.Categories()) {
		const int parent = int(_pickerCategories.size());
		addCategory(category.name, category.id, L"", category.description, -1,
					!category.subcategories.empty());
		for (const auto &[name, description] : category.subcategories)
			addCategory(name, category.id, name, description, parent, false);
	}
	addCategory(PickerString(L"EffectPicker_CustomCategory"), L"custom", L"",
		PickerString(L"EffectPicker_CustomCategoryDescription"), -1, false);
	_pickerCategoryScroll = PickerScroll(categories);
	_pickerCategoryPane.Child(_pickerCategoryScroll);
	Grid right;
	Row(right, 1, GridUnitType::Auto);
	Row(right, 1, GridUnitType::Auto);
	Row(right, 1, GridUnitType::Star);
	_pickerListPane.Child(right);
	_pickerSearch = TextBox();
	_pickerSearch.FontSize(13);
	_pickerSearch.PlaceholderText(PickerString(L"EffectPicker_SearchPlaceholder"));
	AutomationProperties::SetName(_pickerSearch, PickerString(L"EffectPicker_SearchAutomationName"));
	right.Children().Append(_pickerSearch);
	_pickerCount = PickerText(L"", 11);
	_pickerCount.Margin({2, 7, 0, 7});
	_pickerCount.Opacity(0.7);
	Grid::SetRow(_pickerCount, 1);
	right.Children().Append(_pickerCount);
	Grid resultsArea;
	Column(resultsArea, 1, GridUnitType::Star);
	Column(resultsArea, 1, GridUnitType::Auto);
	Grid::SetRow(resultsArea, 2);
	right.Children().Append(resultsArea);
	_pickerResults = StackPanel();
	_pickerListScroll = PickerScroll(_pickerResults);
	resultsArea.Children().Append(_pickerListScroll);
	Grid letters;
	letters.VerticalAlignment(VerticalAlignment::Top);
	Column(letters, 18, GridUnitType::Pixel);
	const auto letterStyle = Resources().Lookup(box_value(L"EffectPickerLetterButtonStyle"))
		.as<Windows::UI::Xaml::Style>();
	for (int i = 0; i < 27; ++i) {
		Row(letters, 18, GridUnitType::Pixel);
		const std::wstring name(1, i == 26 ? L'#' : wchar_t(L'A' + i));
		Button button;
		button.Style(letterStyle);
		button.Content(box_value(name));
		AutomationProperties::SetName(button, PickerString(L"EffectPicker_JumpTo") + name);
		button.Click([weak, i](auto const&, auto const&) {
			if (auto page = weak.get()) page->_JumpEffectPickerLetter(i);
		});
		button.KeyDown([weak, i](auto const&, KeyRoutedEventArgs const& args) {
			const auto page = weak.get();
			if (!page) return;
			const auto key = args.Key();
			const int step = key == VirtualKey::Down ? 1 : key == VirtualKey::Up ? -1 : 0;
			if (!step) return;
			for (int next = i + step; next >= 0 && next < 27; next += step) {
				if (page->_pickerLetters[next].IsEnabled()) {
					page->_pickerLetters[next].Focus(FocusState::Keyboard);
					break;
				}
			}
			args.Handled(true);
		});
		Grid::SetRow(button, i);
		letters.Children().Append(button);
		_pickerLetters.push_back(button);
	}
	// Keep one column even on a constrained monitor: scroll the narrow index
	// instead of changing its reading order or opening a second letter panel.
	auto indexScroll = PickerScroll(letters);
	indexScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Hidden);
	indexScroll.Margin({6, 0, 0, 0});
	Grid::SetColumn(indexScroll, 1);
	AutomationProperties::SetName(indexScroll, PickerString(L"EffectPicker_AlphabetIndexAutomationName"));
	resultsArea.Children().Append(indexScroll);
	_pickerSearch.TextChanged([weak](auto const &, auto const &) {
		if (const auto page = weak.get(); page && !page->_pickerChangingCategory)
			page->_RefreshEffectPicker();
	});
	_pickerCategoryPane.Style(Resources().Lookup(box_value(L"EffectPickerCategoryPaneStyle")).as<Windows::UI::Xaml::Style>());
	_pickerListPane.Style(Resources().Lookup(box_value(L"EffectPickerPaneStyle")).as<Windows::UI::Xaml::Style>());
	_pickerDetailPane.Style(Resources().Lookup(box_value(L"EffectPickerDetailPaneStyle")).as<Windows::UI::Xaml::Style>());
	_UpdateEffectPickerColors();
	_effectPicker = Flyout();
	_effectPicker.ShouldConstrainToRootBounds(false);
	_effectPicker.Content(_pickerRoot);
	_effectPicker.FlyoutPresenterStyle(Resources().Lookup(box_value(L"EffectPickerPresenterStyle")).as<Windows::UI::Xaml::Style>());
	_effectPicker.Opened([weak](auto const &, auto const &) {
		if (auto page = weak.get()) {
			page->_pickerSearch.Focus(FocusState::Programmatic);
			page->_QueueEffectPickerDetailsHint();
		}
	});
	_effectPicker.Closed([weak](auto const &, auto const &) {
		if (auto page = weak.get()) {
			page->_pickerMode = nullptr;
		}
	});
}
void ScalingModesPage::_SetEffectCategoryExpanded(size_t index, bool expanded) {
	if (index >= _pickerCategories.size() || !_pickerCategories[index].hasChildren)
		return;
	auto &item = _pickerCategories[index];
	const double anchorTop = item.container.TransformToVisual(_pickerCategoryScroll).TransformPoint({0, 0}).Y;
	const auto focused = FocusManager::GetFocusedElement(XamlRoot()).try_as<DependencyObject>();
	bool focusHidden = false;
	item.expanded = expanded;
	item.icon.Expanded(expanded);
	for (auto &child : _pickerCategories) {
		if (child.parent == int(index)) {
			focusHidden |= !expanded && focused && XamlHelper::ContainsControl(child.container, focused);
			child.container.Visibility(expanded ? Visibility::Visible : Visibility::Collapsed);
		}
	}
	if (focusHidden) item.button.Focus(FocusState::Keyboard);
	_pickerRoot.UpdateLayout();
	const double moved = item.container.TransformToVisual(_pickerCategoryScroll).TransformPoint({0, 0}).Y - anchorTop;
	_pickerCategoryScroll.ChangeView(nullptr, std::max(0.0, _pickerCategoryScroll.VerticalOffset() + moved), nullptr, true);
	_UpdateEffectPickerColors();
}
void ScalingModesPage::_UpdateEffectPickerColors() {
	const bool searching = _pickerSearch && !NormalizeEffectSearch(_pickerSearch.Text()).empty();
	for (auto& item : _pickerCategories) {
		const bool selected = !searching && item.category == _pickerCategory && item.subcategory == _pickerSubcategory;
		const bool hiddenSelection = !searching && item.hasChildren && !item.expanded &&
			item.category == _pickerCategory && !_pickerSubcategory.empty();
		item.container.Style(Resources().Lookup(box_value(selected || hiddenSelection
			? L"EffectPickerSelectedCategoryStyle" : L"EffectPickerCategoryStyle")).as<Windows::UI::Xaml::Style>());
		item.selectionMark.Visibility(selected || hiddenSelection ? Visibility::Visible : Visibility::Collapsed);
		std::wstring status = selected ? PickerString(L"EffectPicker_Selected") :
			hiddenSelection ? PickerString(L"EffectPicker_ChildSelected") : L"";
		if (item.hasChildren) {
			if (!status.empty()) status += PickerString(L"EffectPicker_NameSeparator");
			status += PickerString(item.expanded ? L"EffectPicker_Expanded" : L"EffectPicker_Collapsed");
		}
		AutomationProperties::SetItemStatus(item.button, status);
	}
}
void ScalingModesPage::_ChooseEffectCategory(std::wstring category, std::wstring subcategory,
											 std::wstring description) {
	_pickerCategory = std::move(category);
	_pickerSubcategory = std::move(subcategory);
	_pickerChangingCategory = true;
	_pickerSearch.Text(L"");
	_pickerChangingCategory = false;
	_RefreshEffectPicker();
	_SetEffectPickerDetails(_pickerDetailTitle.Text(), description);
}
void ScalingModesPage::_RefreshEffectPicker(std::wstring anchor) {
	double anchorTop = 0;
	Windows::UI::Xaml::FocusState anchorFocus = FocusState::Programmatic;
	for (const auto& row : _pickerRows) {
		if (row.entry.key == anchor) {
			anchorTop = row.button.TransformToVisual(_pickerResults).TransformPoint({0, 0}).Y
				- _pickerListScroll.VerticalOffset();
			if (row.button.FocusState() == FocusState::Keyboard) anchorFocus = FocusState::Keyboard;
			break;
		}
	}
	const std::wstring query(_pickerSearch.Text());
	const bool searching = !NormalizeEffectSearch(query).empty();
	if (_pickerLastQuery != query) {
		_pickerSearchCollapsedFamilies.clear();
		_pickerLastQuery = query;
	}
	const auto tree = BuildEffectPickerTree(_pickerEntries, _pickerCategory, _pickerSubcategory,
		query, _pickerExpandedFamilies, _pickerSearchCollapsedFamilies);
	_pickerResults.Children().Clear();
	_pickerRows.clear();
	_pickerLetterTargets.fill({});
	const auto weak = get_weak();
	for (const auto& entry : tree.rows) {
		PickerRow row;
		row.entry = entry;
		row.button = _EffectPickerButton(nullptr);
		SetEffectPickerRowMargin(row.button, entry.depth * 14.0);
		Grid content;
		Column(content, 20, GridUnitType::Pixel);
		Column(content, 1, GridUnitType::Star);
		if (entry.IsFamily()) {
			auto icon = MakeEffectPickerToggleIcon(row.button);
			icon.Expanded(entry.expanded);
			icon.root.VerticalAlignment(VerticalAlignment::Top);
			icon.root.Margin({0, 4, 6, 0});
			content.Children().Append(icon.root);
		}
		StackPanel text;
		Grid label;
		Column(label, 1, GridUnitType::Star);
		Column(label, 1, GridUnitType::Auto);
		auto name = PickerText(entry.name, 13, true);
		name.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
		label.Children().Append(name);
		if (entry.IsFamily()) {
			auto count = PickerText(std::to_wstring(entry.count), 10);
			count.Opacity(0.7); count.Margin({8, 0, 0, 0});
			count.VerticalAlignment(VerticalAlignment::Center);
			Grid::SetColumn(count, 1);
			label.Children().Append(count);
			AutomationProperties::SetItemStatus(row.button,
				PickerString(entry.expanded ? L"EffectPicker_Expanded" : L"EffectPicker_Collapsed"));
		}
		text.Children().Append(label);
		if (!entry.summary.empty()) {
			auto summary = PickerText(entry.summary, 11, true);
			summary.Opacity(0.75); summary.Margin({0, 3, 0, 0});
			text.Children().Append(summary);
		}
		Grid::SetColumn(text, 1);
		content.Children().Append(text);
		row.button.Content(content);
		AutomationProperties::SetName(row.button,
			entry.IsFamily() ? PickerEffectCount(entry.name, entry.count) : entry.name);
		const auto problem = !entry.IsFamily() && _pickerMode
			? get_self<ScalingModeItem>(_pickerMode)->EffectAddProblem(hstring(entry.effectId)) : hstring{};
		AutomationProperties::SetHelpText(row.button, problem.empty() ? hstring(entry.summary) : problem);
		auto show = [weak, key = entry.key](auto const&, auto const&) {
			if (auto page = weak.get()) page->_ShowEffectPickerDetails(key);
		};
		// Physical pointer movement avoids reselecting a row moved under a stationary cursor.
		row.button.PointerMoved(show);
		row.button.GotFocus(show);
		row.button.Click([weak, key = entry.key, id = entry.effectId](auto const&, auto const&) {
			if (auto page = weak.get()) {
				if (id.empty()) page->_ToggleEffectFamily(key);
				else page->_AddPickedEffect(id);
			}
		});
		row.button.KeyDown([weak, key = entry.key](auto const&, KeyRoutedEventArgs const& args) {
			if (auto page = weak.get()) page->_EffectPickerRowKeyDown(key, args);
		});
		if (entry.depth == 0) {
			auto& target = _pickerLetterTargets[EffectPickerLetter(entry.name)];
			if (target.empty()) target = entry.key;
		}
		_pickerResults.Children().Append(row.button);
		_pickerRows.push_back(std::move(row));
	}
	std::wstring scope = PickerString(searching ? L"EffectPicker_SearchAllCategories" : L"EffectPicker_All");
	if (!searching && anchor.empty()) {
		for (const auto& item : _pickerCategories) {
			if (item.category != _pickerCategory || item.subcategory != _pickerSubcategory) continue;
			scope = item.parent >= 0 ? _pickerCategories[item.parent].name + L" › " + item.name : item.name;
			_SetEffectPickerDetails(item.name, item.description);
			break;
		}
	} else if (!searching) {
		for (const auto& item : _pickerCategories)
			if (item.category == _pickerCategory && item.subcategory == _pickerSubcategory)
				scope = item.parent >= 0 ? _pickerCategories[item.parent].name + L" › " + item.name : item.name;
	}
	_pickerCount.Text(scope + L" · " + std::to_wstring(tree.effectCount));
	if (!tree.effectCount) {
		_SetEffectPickerDetails(PickerString(L"EffectPicker_NoMatches"),
			PickerString(L"EffectPicker_NoMatchesDescription"));
	} else if (searching && anchor.empty()) {
		_ShowEffectPickerDetails(_pickerRows.front().entry.key);
	}
	_UpdateEffectPickerColors();
	_UpdateEffectPickerIndex();
	_pickerRoot.UpdateLayout();
	if (anchor.empty()) {
		_pickerListScroll.ChangeView(nullptr, 0.0, nullptr, true);
		_pickerDetailScroll.ChangeView(nullptr, 0.0, nullptr, true);
	} else {
		for (const auto& row : _pickerRows) {
			if (row.entry.key != anchor) continue;
			row.button.Focus(anchorFocus);
			const double top = row.button.TransformToVisual(_pickerResults).TransformPoint({0, 0}).Y;
			_pickerListScroll.ChangeView(nullptr, std::max(0.0, top - anchorTop), nullptr, true);
			break;
		}
	}
}

void ScalingModesPage::_ShowEffectPickerDetails(std::wstring_view key) {
	const auto it = std::ranges::find(_pickerRows, key, [](const auto& row) { return std::wstring_view(row.entry.key); });
	if (it == _pickerRows.end()) return;
	const auto& row = it->entry;
	std::wstring text = row.summary;
	if (!row.IsFamily()) {
		const auto entry = std::ranges::find(_pickerEntries, row.effectId, &EffectPickerEntry::id);
		if (entry != _pickerEntries.end()) text = entry->details;
		if (_pickerMode) {
			const auto problem = get_self<ScalingModeItem>(_pickerMode)->EffectAddProblem(hstring(row.effectId));
			if (!problem.empty()) text = std::wstring(problem) + L"\n\n" + text;
		}
	}
	_SetEffectPickerDetails(row.name, text);
}

void ScalingModesPage::_SetEffectPickerDetails(std::wstring_view title, std::wstring_view text) {
	if (std::wstring_view(_pickerDetailTitle.Text()) == title && std::wstring_view(_pickerDetails.Text()) == text) return;
	_pickerDetailTitle.Text(title);
	_pickerDetails.Text(text);
	_pickerDetailScroll.ChangeView(nullptr, 0.0, nullptr, true);
	_QueueEffectPickerDetailsHint();
}

void ScalingModesPage::_QueueEffectPickerDetailsHint() {
	if (_pickerDetailsHintQueued || !_pickerMode) return;
	_pickerDetailsHintQueued = true;
	if (!DispatcherQueue::GetForCurrentThread().TryEnqueue(DispatcherQueuePriority::Low, [weak = get_weak()] {
		if (auto page = weak.get()) {
			page->_pickerDetailsHintQueued = false;
			if (page->_pickerMode && page->_pickerRoot && page->_pickerRoot.IsLoaded())
				page->_UpdateEffectPickerDetailsHint();
		}
	})) _pickerDetailsHintQueued = false;
}

void ScalingModesPage::_UpdateEffectPickerDetailsHint() {
	const bool overflow = EffectPickerDetailsOverflow(
		std::max(_pickerDetailScroll.ExtentHeight(), _pickerDetailContent.ActualHeight()),
		_pickerDetailArea.ActualHeight());
	_pickerDetailHint.Visibility(overflow ? Visibility::Visible : Visibility::Collapsed);
}

void ScalingModesPage::_EffectPickerPointerWheelChanged(IInspectable const&, PointerRoutedEventArgs const& args) {
	if (!_pickerMode) return;
	const auto properties = args.GetCurrentPoint(_pickerRoot).Properties();
	if (!IsEffectPickerDetailsWheel(
		(args.KeyModifiers() & Windows::System::VirtualKeyModifiers::Control) != Windows::System::VirtualKeyModifiers::None,
		properties.IsHorizontalMouseWheel(), properties.MouseWheelDelta())) return;
	// Consume Ctrl+wheel at both ends and for short descriptions too, so it
	// never falls through to the page or changes the current browsing position.
	args.Handled(true);
	_pickerDetailScroll.ChangeView(nullptr, EffectPickerDetailsWheelOffset(
		_pickerDetailScroll.VerticalOffset(), _pickerDetailScroll.ScrollableHeight(), properties.MouseWheelDelta()),
		nullptr, true);
}

void ScalingModesPage::_ToggleEffectFamily(std::wstring_view key) {
	const auto it = std::ranges::find(_pickerRows, key, [](const auto& row) { return std::wstring_view(row.entry.key); });
	if (it == _pickerRows.end() || !it->entry.IsFamily()) return;
	const auto stableKey = it->entry.key;
	const bool searching = !NormalizeEffectSearch(_pickerSearch.Text()).empty();
	auto& states = searching ? _pickerSearchCollapsedFamilies : _pickerExpandedFamilies;
	if (searching ? it->entry.expanded : !it->entry.expanded) states.insert(stableKey);
	else states.erase(stableKey);
	_RefreshEffectPicker(stableKey);
}

void ScalingModesPage::_EffectPickerRowKeyDown(std::wstring_view key, KeyRoutedEventArgs const& args) {
	const auto it = std::ranges::find(_pickerRows, key, [](const auto& row) { return std::wstring_view(row.entry.key); });
	if (it == _pickerRows.end()) return;
	const size_t index = size_t(it - _pickerRows.begin());
	const auto entry = it->entry;
	const auto pressed = args.Key();
	if (pressed == VirtualKey::Enter) {
		args.Handled(true);
		if (entry.IsFamily()) _ToggleEffectFamily(entry.key);
		else _AddPickedEffect(entry.effectId);
	} else if (pressed == VirtualKey::Down || pressed == VirtualKey::Up) {
		const int next = int(index) + (pressed == VirtualKey::Down ? 1 : -1);
		if (next >= 0 && next < int(_pickerRows.size())) _pickerRows[next].button.Focus(FocusState::Keyboard);
		args.Handled(true);
	} else if (pressed == VirtualKey::Right && entry.IsFamily()) {
		if (!entry.expanded) _ToggleEffectFamily(entry.key);
		else if (index + 1 < _pickerRows.size()) _pickerRows[index + 1].button.Focus(FocusState::Keyboard);
		args.Handled(true);
	} else if (pressed == VirtualKey::Left) {
		if (entry.IsFamily() && entry.expanded) _ToggleEffectFamily(entry.key);
		else for (const auto& row : _pickerRows)
			if (row.entry.key == entry.parent) { row.button.Focus(FocusState::Keyboard); break; }
		args.Handled(true);
	}
	// Space uses Button's normal press/release Click behavior, including families.
}

void ScalingModesPage::_UpdateEffectPickerIndex() {
	for (int i = 0; i < 27; ++i) {
		_pickerLetters[i].IsEnabled(!_pickerLetterTargets[i].empty());
	}
}

void ScalingModesPage::_JumpEffectPickerLetter(int letter) {
	if (letter < 0 || letter >= 27 || _pickerLetterTargets[letter].empty()) return;
	_pickerRoot.UpdateLayout();
	for (const auto& row : _pickerRows) {
		if (row.entry.key != _pickerLetterTargets[letter]) continue;
		row.button.Focus(FocusState::Keyboard);
		const double top = row.button.TransformToVisual(_pickerResults).TransformPoint({0, 0}).Y;
		_pickerListScroll.ChangeView(nullptr, top, nullptr, true);
		break;
	}
}

void ScalingModesPage::_AddPickedEffect(std::wstring_view id) {
	if (!_pickerMode) return;
	const auto entry = std::ranges::find(_pickerEntries, id, [](const auto& effect) { return std::wstring_view(effect.id); });
	if (entry == _pickerEntries.end()) return;
	const auto mode = _pickerMode;
	const hstring stableId(entry->id);
	if (!get_self<ScalingModeItem>(mode)->EffectAddProblem(stableId).empty()) {
		_ShowEffectPickerDetails(L"effect:" + entry->id);
		return;
	}
	_pickerMode = nullptr;
	_effectPicker.Hide();
	get_self<ScalingModeItem>(mode)->AddEffect(stableId);
}

} // namespace winrt::Magpie::implementation
