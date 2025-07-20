// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CommandPalette.h"
#include <LibraryResources.h>

#include "CommandPaletteItems.h"
#include "fzf/fzf.h"

#include "FilteredCommand.g.cpp"

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    int32_t FilteredCommand::Ordinal()
    {
        return _ordinal;
    }

    FilteredCommand::FilteredCommand(const winrt::TerminalApp::IPaletteItem& item) :
        FilteredCommand(item, 0, false)
    {
    }

    // This class is a wrapper of PaletteItem, that is used as an item of a filterable list in CommandPalette.
    // It manages a highlighted text that is computed by matching search filter characters to item name

    FilteredCommand::FilteredCommand(const winrt::TerminalApp::IPaletteItem& item, int32_t ordinal, bool searchDescription)
    {
        // Actually implement the ctor in _constructFilteredCommand
        _ordinal = ordinal;
        _searchDescription = searchDescription;
        _constructFilteredCommand(item);
    }

    // We need to actually implement the ctor in a separate helper. This is
    // because we have a FilteredTask class which derives from FilteredCommand.
    // HOWEVER, for cppwinrt ~ r e a s o n s ~, it doesn't actually derive from
    // FilteredCommand directly, so we can't just use the FilteredCommand ctor
    // directly in the base class.
    void FilteredCommand::_constructFilteredCommand(const winrt::TerminalApp::IPaletteItem& item)
    {
        if (_searchDescription)
        {
            if (auto cmd = item.try_as<ActionPaletteItem>())
            {
                Description(cmd->Command().Description());
                auto range = cmd->Command().Range();
                _scrollbackRange.Start = range.Start;
                _scrollbackRange.End = range.End;
            }
        }

        _Item = item;
        _Weight = 0;

        _update();

        // Recompute the highlighted name if the item name changes
        _itemChangedRevoker = _Item.as<winrt::Windows::UI::Xaml::Data::INotifyPropertyChanged>().PropertyChanged(winrt::auto_revoke, [weakThis{ get_weak() }](auto& /*sender*/, auto& e) {
            auto filteredCommand{ weakThis.get() };
            if (filteredCommand && e.PropertyName() == L"Name")
            {
                filteredCommand->_update();
            }
        });
    }

    void FilteredCommand::UpdateFilter(std::shared_ptr<fzf::matcher::Pattern> pattern)
    {
        // If the filter was not changed we want to prevent the re-computation of matching
        // that might result in triggering a notification event
        if (pattern != _pattern)
        {
            _pattern = pattern;
            _update();
        }
    }

    static std::tuple<std::vector<winrt::TerminalApp::HighlightedRun>, int32_t> _matchedSegmentsAndWeight(const std::shared_ptr<fzf::matcher::Pattern>& pattern, const winrt::hstring& haystack)
    {
        std::vector<winrt::TerminalApp::HighlightedRun> segments;
        int32_t weight = 0;

        if (pattern && !pattern->terms.empty())
        {
            if (auto match = fzf::matcher::Match(haystack, *pattern.get()); match)
            {
                auto& matchResult = *match;
                weight = matchResult.Score;
                segments.resize(matchResult.Runs.size());
                std::transform(matchResult.Runs.begin(), matchResult.Runs.end(), segments.begin(), [](auto&& run) -> winrt::TerminalApp::HighlightedRun {
                    return { run.Start, run.End };
                });
            }
        }
        return { std::move(segments), weight };
    }


    void FilteredCommand::_update()
    {
        auto description = Description();
        auto [segments, weight] = _searchDescription && !description.empty() ? 
            _matchedSegmentsAndWeight(_pattern, description) :
            _matchedSegmentsAndWeight(_pattern, _Item.Name());

        // Calculate HighlightedSubName first (intersection of filter highlights and scrollback range)
        std::vector<winrt::TerminalApp::HighlightedRun> intersectionHighlights;
        if (_scrollbackRange.End > _scrollbackRange.Start && !segments.empty())
        {
            if (_searchDescription && !description.empty())
            {
                // When searching description, segments are relative to description (full row)
                // so we can directly intersect with scrollback range
                const auto rangeStart = static_cast<uint64_t>(_scrollbackRange.Start);
                const auto rangeEnd = static_cast<uint64_t>(_scrollbackRange.End);
                
                for (const auto& segment : segments)
                {
                    const auto intersectStart = std::max(segment.Start, rangeStart);
                    const auto intersectEnd = std::min(segment.End, rangeEnd);
                    
                    if (intersectStart <= intersectEnd)
                    {
                        const auto offsetStart = intersectStart - rangeStart;
                        const auto offsetEnd = intersectEnd - rangeStart;
                        
                        const auto itemNameLength = static_cast<uint64_t>(_Item.Name().size());
                        if (offsetStart < itemNameLength)
                        {
                            auto end = std::min(offsetEnd, itemNameLength);
                            weight += static_cast<int>((end - offsetStart) * 8);
                            intersectionHighlights.push_back({ 
                                offsetStart, 
                                std::min(offsetEnd, itemNameLength)
                            });
                        }
                    }
                }
            }
            else
            {
                // When searching item name, segments are already relative to item name
                // so we just use them directly (they're already within the scrollback range)
                for (const auto& segment : segments)
                {
                    const auto itemNameLength = static_cast<uint64_t>(_Item.Name().size());
                    if (segment.Start < itemNameLength)
                    {
                        intersectionHighlights.push_back({ 
                            segment.Start, 
                            std::min(segment.End, itemNameLength)
                        });
                    }
                }
            }
        }

        // Set filter highlights (NameHighlights)
        if (segments.empty())
        {
            NameHighlights(nullptr);
        }
        else
        {
            NameHighlights(winrt::single_threaded_vector(std::move(segments)));
        }

        // Set HighlightedSubName 
        if (!intersectionHighlights.empty())
        {
            HighlightedSubName(winrt::single_threaded_vector(std::move(intersectionHighlights)));
        }
        else
        {
            HighlightedSubName(nullptr);
        }

        Weight(weight);
    }

    // Function Description:
    // - Implementation of Compare for FilteredCommand interface.
    // Compares first instance of the interface with the second instance, first by weight, then by name.
    // In the case of a tie prefers the first instance.
    // Arguments:
    // - other: another instance of FilteredCommand interface
    // Return Value:
    // - Returns true if the first is "bigger" (aka should appear first)
    int FilteredCommand::Compare(const winrt::TerminalApp::FilteredCommand& first, const winrt::TerminalApp::FilteredCommand& second)
    {
        auto firstWeight{ first.Weight() };
        auto secondWeight{ second.Weight() };

        if (firstWeight == secondWeight)
        {
            if (first.Ordinal() != second.Ordinal())
            {
                return first.Ordinal() < second.Ordinal();
            }
            const auto firstName = first.Item().Name();
            const auto secondName = second.Item().Name();
            return til::compare_linguistic_insensitive(firstName, secondName) < 0;
        }

        return firstWeight > secondWeight;
    }
}
