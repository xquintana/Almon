// TopStacksManager.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "stdafx.h"
#include "TopStacks.h"
#include "Utils.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TopStacks

void TopStacks::Add(const CallStack& stack, CallStackInfo* pInfo)
{
	AcquireSRWLockExclusive(&m_lckTopStackMap);
	TopStackMapIter it = m_topStackMap.find(stack);
	if (it == m_topStackMap.end())
	{
		TopStackInternal topInfo;
		auto iterColor = m_pColorMap->find(stack);
		if (iterColor != m_pColorMap->end())
		{
			topInfo.color = iterColor->second;
		}
		else
		{
			topInfo.color = { m_counter++ , m_colorRandomizer.GetRGBColor() };			
			m_pColorMap->emplace(stack, topInfo.color);
		}
		topInfo.pStackInfo = pInfo;
		m_topStackMap[stack] = topInfo;
	}
	ReleaseSRWLockExclusive(&m_lckTopStackMap);
}

void TopStacks::UpdateTopItems(std::vector<TopStack>& topStacks, bool bRetrieve)
{
	AcquireSRWLockExclusive(&m_lckTopStackMap);
	if (m_topStackMap.size() == 0)
	{
		ReleaseSRWLockExclusive(&m_lckTopStackMap);
		return;
	}

	// Sort call stacks	
	std::vector<SortedStack> sorted;
	sorted.reserve(m_topStackMap.size());
	for (const auto& elem : m_topStackMap)
		sorted.emplace_back(elem.first, elem.second);
	std::sort(sorted.begin(), sorted.end(), m_comparator);

	// Remove the exceeding elements and retrieve the remaining ones if required.
	for (size_t i = 0; i < sorted.size(); i++)
	{
		if (i >= m_maxSize)
			m_topStackMap.erase(sorted[i].stack);
		else if (bRetrieve)
			topStacks.push_back(sorted[i].info);
	}
	ReleaseSRWLockExclusive(&m_lckTopStackMap);
}

void TopStacks::Clear()
{
	AcquireSRWLockExclusive(&m_lckTopStackMap);
	m_topStackMap.clear();
	ReleaseSRWLockExclusive(&m_lckTopStackMap);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TopStacksManager

TopStacksManager::TopStacksManager()
{
	InitializeSRWLock(&m_lockTopStacks);
	m_topStacksBytes.Init(&m_colorMap, [](const SortedStack& elem1, const SortedStack& elem2)
		{
			return elem1.info.stackInfo.numBytes > elem2.info.stackInfo.numBytes;
		});
	m_topStacksAllocs.Init(&m_colorMap, [](const SortedStack& elem1, const SortedStack& elem2)
		{
			return elem1.info.stackInfo.numAllocs > elem2.info.stackInfo.numAllocs;
		});
	m_topStacksUsage.Init(&m_colorMap, [](const SortedStack& elem1, const SortedStack& elem2)
		{
			return elem1.info.stackInfo.numBytesInUse > elem2.info.stackInfo.numBytesInUse;
		});
}

TopStacksManager::~TopStacksManager()
{
	Clear();
}

void TopStacksManager::Add(const CallStack& stack, CallStackInfo* pInfo)
{
	AcquireSRWLockExclusive(&m_lockTopStacks);
	m_topStacksBytes.Add(stack, pInfo);
	m_topStacksAllocs.Add(stack, pInfo);
	m_topStacksUsage.Add(stack, pInfo);
	ReleaseSRWLockExclusive(&m_lockTopStacks);
}

void TopStacksManager::SetMaxSize(int maxSize)
{
	AcquireSRWLockExclusive(&m_lockTopStacks);
	m_topStacksBytes.SetMaxSize(maxSize);
	m_topStacksAllocs.SetMaxSize(maxSize);
	m_topStacksUsage.SetMaxSize(maxSize);
	ReleaseSRWLockExclusive(&m_lockTopStacks);
};

void TopStacksManager::Clear()
{
	AcquireSRWLockExclusive(&m_lockTopStacks);
	m_colorMap.clear();
	m_topStacksBytes.Clear();
	m_topStacksAllocs.Clear();
	m_topStacksUsage.Clear();
	ReleaseSRWLockExclusive(&m_lockTopStacks);
}

void TopStacksManager::UpdateTopStacks(std::vector<TopStack>& topStacks, SortingType sortingType)
{
	topStacks.clear();
	AcquireSRWLockExclusive(&m_lockTopStacks);
	m_topStacksBytes.UpdateTopItems(topStacks, sortingType == SortingType::Size);
	m_topStacksAllocs.UpdateTopItems(topStacks, sortingType == SortingType::NumAllocs);
	m_topStacksUsage.UpdateTopItems(topStacks, sortingType == SortingType::Usage);
	ReleaseSRWLockExclusive(&m_lockTopStacks);
}