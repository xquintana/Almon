#pragma once

#include "CommonDefs.h"
#include <random>


struct TopStackColor
{
	DWORD id;
	COLORREF rgb;
};

struct TopStackInternal
{
	CallStackInfo* pStackInfo;
	TopStackColor color;
};

struct TopStack
{
	TopStack(TopStackInternal info) : stackInfo{ *(info.pStackInfo) }, color{ info.color } {};
	CallStackInfo stackInfo;
	TopStackColor color;
};

struct SortedStack
{
	SortedStack(CallStack stack, TopStack info) : stack(stack), info(info) {}
	SortedStack(CallStack stack, TopStackInternal info) : stack(stack), info(info) {}
	CallStack stack;
	TopStack info;
};

using TopStackMap = std::unordered_map<CallStack, TopStackInternal, CallStackHasher>;
using TopStackMapIter = TopStackMap::iterator;
using ColorMap = std::unordered_map<CallStack, TopStackColor, CallStackHasher>;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TopStacks
// Contains a sorted collection of call stacks.

class TopStacks
{
	// ColorRandomizer	
	// Returns random colors.
	// These colors are used for the background of the call stacks in the real-time view.

	class ColorRandomizer
	{
	public:
		ColorRandomizer() : m_generator{ m_seed }, m_distribution{ 0, (255 - m_offset) } {};
		COLORREF GetRGBColor() { return RGB(GetColorComponent(), GetColorComponent(), GetColorComponent()); }

	protected:
		BYTE GetColorComponent() { return static_cast<BYTE>(m_distribution(m_generator) + m_offset); }

	protected:
		unsigned int m_seed{ 100 };
		int m_offset{ 120 };
		std::default_random_engine m_generator;
		std::uniform_int_distribution<> m_distribution;
	};

	using Comparator = std::function<bool(SortedStack, SortedStack)>;

public:
	void Init(ColorMap* pColorMap, Comparator comparator) { m_pColorMap = pColorMap; m_comparator = comparator; InitializeSRWLock(&m_lckTopStackMap); }
	void SetMaxSize(int maxSize) { m_maxSize = maxSize; };
	void Add(const CallStack& stack, CallStackInfo* pInfo);
	void UpdateTopItems(std::vector<TopStack>& topStacks, bool bRetrieve); // Updates
	void Clear();

protected:
	TopStackMap m_topStackMap;
	SRWLOCK m_lckTopStackMap;
	inline static DWORD m_counter{};
	inline static ColorRandomizer m_colorRandomizer{};
	Comparator m_comparator{ nullptr };
	int m_maxSize{};
	ColorMap* m_pColorMap;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TopStacksManager
// Updates three instances of TopStacks sorted by number of bytes, number of allocs and number of bytes in use.
// Determines the background color of each stack rendered the top stacks view.

class TopStacksManager
{
public:
	enum class SortingType
	{
		Size,
		NumAllocs,
		Usage
	};

	TopStacksManager();
	~TopStacksManager();

	void SetMaxSize(int maxSize);
	void Add(const CallStack& stack, CallStackInfo* pInfo);
	void UpdateTopStacks(std::vector<TopStack>& topStacks, SortingType sortingType);
	void SelectItem(DWORD id) { m_selItemId = id; }
	DWORD GetSelectedItem() { return m_selItemId; }
	void Clear();

protected:
	DWORD m_selItemId{};	// Index of the selected stack in the interface.
	TopStacks m_topStacksBytes;  // Keeps the top stacks sorted by number of bytes.
	TopStacks m_topStacksAllocs; // Keeps the top stacks sorted by number of allocs.
	TopStacks m_topStacksUsage;  // Keeps the top stacks sorted by number of bytes in use.
	SRWLOCK m_lockTopStacks; // Protects the access to the 'TopStacks' members.
	ColorMap m_colorMap; // Associates a color to each call stack. Shared by all 'TopStacks' members.
};

