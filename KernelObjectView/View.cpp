// View.cpp : implementation of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "View.h"

BOOL CView::PreTranslateMessage(MSG* pMsg) {
	return FALSE;
}

bool CView::TogglePause() {
	m_Paused = !m_Paused;
	if (m_Paused)
		KillTimer(1);
	else
		SetTimer(1, m_Interval, nullptr);
	return m_Paused;
}

void CView::SetInterval(int interval) {
	m_Interval = interval;
}

LRESULT CView::OnCreate(UINT, WPARAM, LPARAM, BOOL &) {
	DefWindowProc();

	InsertColumn(0, L"Name", LVCFMT_LEFT, 130);
	InsertColumn(1, L"Index", LVCFMT_RIGHT, 50);
	InsertColumn(2, L"Objects", LVCFMT_RIGHT, 100);
	InsertColumn(3, L"Handles", LVCFMT_RIGHT, 100);
	InsertColumn(4, L"Peak Objects", LVCFMT_RIGHT, 100);
	InsertColumn(5, L"Peak Handles", LVCFMT_RIGHT, 100);
	InsertColumn(6, L"Pool Type", LVCFMT_LEFT, 110);
	InsertColumn(7, L"Default Paged Charge", LVCFMT_RIGHT, 110);
	InsertColumn(8, L"Default Non-Paged Charge", LVCFMT_RIGHT, 110);

	SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP | LVS_EX_HEADERDRAGDROP, 0);

	auto count = m_ObjectManager.EnumObjectTypes();
	SetItemCount(count);
	m_Items = m_ObjectManager.GetObjectTypes();

	SetTimer(1, m_Interval, nullptr);

	return 0;
}

LRESULT CView::OnTimer(UINT, WPARAM wParam, LPARAM, BOOL &) {
	if (wParam == 1) {
		KillTimer(1);
		m_ObjectManager.EnumObjectTypes();
		if (m_SortColumn >= 0)
			DoSort();
		LockWindowUpdate();
		RedrawItems(GetTopIndex(), GetCountPerPage() + GetTopIndex());
		LockWindowUpdate(FALSE);

		if (!m_Paused)
			SetTimer(1, m_Interval, nullptr);
	}
	return 0;
}

LRESULT CView::OnGetDispInfo(int, LPNMHDR hdr, BOOL &) {
	auto lv = reinterpret_cast<NMLVDISPINFO*>(hdr);
	auto& item = lv->item;
	auto index = item.iItem;
	auto col = item.iSubItem;
	auto& data = GetItem(index);

	if (lv->item.mask & LVIF_TEXT) {
		switch (col) {
			case 0:		// name
				item.pszText = (PWSTR)(PCWSTR)data->TypeName;
				break;

			case 1:		// index
				::StringCchPrintf(item.pszText, item.cchTextMax, L"%d", data->TypeIndex);
				break;

			case 2:		// objects
				::StringCchPrintf(item.pszText, item.cchTextMax, L"%d", data->TotalNumberOfObjects);
				break;

			case 3:		// handles
				::StringCchPrintf(item.pszText, item.cchTextMax, L"%d", data->TotalNumberOfHandles);
				break;

			case 4:		// peak objects
				::StringCchPrintf(item.pszText, item.cchTextMax, L"%d", data->HighWaterNumberOfObjects);
				break;

			case 5:		// peak handles
				::StringCchPrintf(item.pszText, item.cchTextMax, L"%d", data->HighWaterNumberOfHandles);
				break;

			case 6:		// name
				item.pszText = (PWSTR)PoolTypeToString(data->PoolType);
				break;

			case 7:		// default non-paged charge
				::StringCchPrintf(item.pszText, item.cchTextMax, L"%d", data->DefaultNonPagedPoolCharge);
				break;

			case 8:		// default paged charge
				::StringCchPrintf(item.pszText, item.cchTextMax, L"%d", data->DefaultPagedPoolCharge);
				break;
		}
	}
	return 0;
}

LRESULT CView::OnColumnClick(int, LPNMHDR hdr, BOOL &) {
	auto lv = (NMLISTVIEW*)hdr;
	auto col = lv->iSubItem;
	auto oldSortColumn = m_SortColumn;
	if (col == m_SortColumn)
		m_SortAscending = !m_SortAscending;
	else {
		m_SortColumn = col;
		m_SortAscending = true;
	}

	HDITEM h;
	h.mask = HDI_FORMAT;
	auto header = GetHeader();
	header.GetItem(m_SortColumn, &h);
	h.fmt = (h.fmt & HDF_JUSTIFYMASK) | HDF_STRING | (m_SortAscending ? HDF_SORTUP : HDF_SORTDOWN);
	header.SetItem(m_SortColumn, &h);

	if (oldSortColumn >= 0 && oldSortColumn != m_SortColumn) {
		h.mask = HDI_FORMAT;
		header.GetItem(oldSortColumn, &h);
		h.fmt = (h.fmt & HDF_JUSTIFYMASK) | HDF_STRING;
		header.SetItem(oldSortColumn, &h);
	}

	DoSort();
	RedrawItems(GetTopIndex(), GetTopIndex() + GetCountPerPage());

	return 0;
}

LRESULT CView::OnFindItem(int, LPNMHDR hdr, BOOL &) {
	auto fi = (NMLVFINDITEM*)hdr;
	if (fi->lvfi.flags & (LVFI_PARTIAL | LVFI_SUBSTRING)) {
		int start = fi->iStart;
		auto count = GetItemCount();
		auto text = fi->lvfi.psz;
		auto len = ::wcslen(text);
		for (int i = start; i < start + count; i++) {
			auto index = i % count;
			if (::_wcsnicmp(m_Items[index]->TypeName, text, len) == 0)
				return index;
		}
	}
	return -1;
}

std::shared_ptr<ObjectType> CView::GetItem(int index) const {
	return m_Items[index];
}

PCWSTR CView::PoolTypeToString(PoolType type) {
	switch (type) {
		case PoolType::NonPagedPool:
			return L"Non Paged";
		case PoolType::PagedPool:
			return L"Paged";
		case PoolType::NonPagedPoolNx:
			return L"Non Paged NX";
		case PoolType::PagedPoolSessionNx:
			return L"Paged Session NX";
	}
	return L"Unknown";
}

bool CView::CompareItems(const std::shared_ptr<ObjectType>& item1, const std::shared_ptr<ObjectType>& item2) const {
	bool result = false;
	switch (m_SortColumn) {
		case 0:		// name
			result = item2->TypeName.CompareNoCase(item1->TypeName) >= 0;
			break;

		case 1:		// index
			result = item2->TypeIndex > item1->TypeIndex;
			break;

		case 2:		// objects
			result = m_SortAscending ? 
				(item2->TotalNumberOfObjects > item1->TotalNumberOfObjects) : 
				(item2->TotalNumberOfObjects < item1->TotalNumberOfObjects);
			return result;

		case 3:		// handles
			result = m_SortAscending ?
				(item2->TotalNumberOfHandles > item1->TotalNumberOfHandles) :
				(item2->TotalNumberOfHandles < item1->TotalNumberOfHandles);
			return result;

		case 4:		// peak objects
			result = m_SortAscending ?
				(item2->HighWaterNumberOfObjects > item1->HighWaterNumberOfObjects) :
				(item2->HighWaterNumberOfObjects < item1->HighWaterNumberOfObjects);
			return result;

		case 5:		// peak handles
			result = m_SortAscending ?
				(item2->HighWaterNumberOfHandles > item1->HighWaterNumberOfHandles) :
				(item2->HighWaterNumberOfHandles < item1->HighWaterNumberOfHandles);
			return result;

		case 6:		// pool type
			result = m_SortAscending ?
				(item2->PoolType > item1->PoolType) : (item1->PoolType > item2->PoolType);
			return result;

		case 7:		// default non-paged charge
			result = m_SortAscending ?
				(item2->DefaultNonPagedPoolCharge > item1->DefaultNonPagedPoolCharge) :
				(item1->DefaultNonPagedPoolCharge > item2->DefaultNonPagedPoolCharge);
			return result;

		case 8:		// default paged charge
			result = m_SortAscending ?
				(item2->DefaultPagedPoolCharge > item1->DefaultPagedPoolCharge) :
				(item1->DefaultPagedPoolCharge > item2->DefaultPagedPoolCharge);
			return result;
	}
	return m_SortAscending ? result : !result;
}

void CView::DoSort() {
	std::sort(m_Items.begin(), m_Items.end(),
		[this](auto& i1, auto& i2) { return CompareItems(i1, i2); });
}