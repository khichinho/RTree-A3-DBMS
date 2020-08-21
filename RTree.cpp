#include "file_manager.h"
#include "buffer_manager.h"
#include "constants.h"
#include "errors.h"

#include <bits/stdc++.h>
// #include <iostream>
// #include <vector>
// #include <climits>

// Dimensions of a point
int d = 2;

// Number of maximum children in a node (width of the tree at a node)
int maxCap = 10;

int rootIndex = -1;
int focusIndex = 0;
// It stores the number of bytes used by the current node
int NodeSize = 0;
int endPageOffset = 0;

using namespace std;

class Point
{
public:
    vector<int> pt;
    Point()
    {
        this->pt.resize(d, INT_MIN);
    }
    Point(int n)
    {
        this->pt.resize(n);
    }
    Point(Point *p)
    {
        for (int i = 0; i < p->pt.size(); i++)
            this->pt[i] = p->pt[i];
    }
}; // namespace stdclassPoint

class MBR
{
public:
    vector<pair<int, int>> mbr;
    MBR()
    {
        this->mbr.resize(d, make_pair(INT_MIN, INT_MIN));
    }
    MBR(int n)
    {
        this->mbr.resize(n, make_pair(INT_MIN, INT_MIN));
    }
    MBR(MBR *m)
    {
        for (int i = 0; i < m->mbr.size(); i++)
        {
            this->mbr[i] = m->mbr[i];
        }
    }
};

class Node
{
public:
    int index;
    int parent_index;
    MBR *bounds;
    vector<int> child_index;
    vector<MBR *> child_bounds;

    Node()
    {
        this->index = -1;
        this->parent_index = -1;
        this->bounds = new MBR(d);
        this->child_index.resize(maxCap);
        this->child_bounds.resize(maxCap);

        for (int i = 0; i < child_bounds.size(); i++)
        {
            this->child_bounds[i] = new MBR(d);
        }
    }

    void set_left_bounds(MBR *m)
    {
        for (int i = 0; i < m->mbr.size(); i++)
        {
            this->bounds->mbr[i].first = m->mbr[i].first;
        }
    }

    void set_right_bounds(MBR *m)
    {
        for (int i = 0; i < m->mbr.size(); i++)
        {
            this->bounds->mbr[i].second = m->mbr[i].second;
        }
    }

    bool is_leaf()
    {
        if (this->child_index[0] == -1)
            return true;
        return false;
    }

    void printNode()
    {
        cout << "-------------------------------------" << endl;
        cout << "[*] INDEX: " << this->index << " PARENT INDEX: " << this->parent_index << endl;
        cout << "[*] BOUNDS: ";
        for (int i = 0; i < d; i++)
        {
            cout << "(" << this->bounds->mbr[i].first << "," << this->bounds->mbr[i].second << ") ";
        }
        cout << "\n[*] CHILDREN: ";
        for (int i = 0; i < maxCap; i++)
        {
            cout << this->child_index[i] << " ";
        }
        cout << "-------------------------------------" << endl;
    }
};

void delete_point(Point *p)
{
    p->pt.clear();
    p->pt.shrink_to_fit();
    delete (p);
}

void delete_mbr(MBR *m)
{
    m->mbr.clear();
    m->mbr.shrink_to_fit();
    delete (m);
}

void delete_node(Node *n)
{
    delete_mbr(n->bounds);
    n->child_index.clear();
    n->child_index.shrink_to_fit();
    for (int i = 0; i < n->child_bounds.size(); i++)
    {
        delete_mbr(n->child_bounds[i]);
    }
    n->child_bounds.clear();
    n->child_bounds.shrink_to_fit();
    delete (n);
}

// store node in char* data
Node *extract_node(char *data)
{
    Node *n = new Node();
    // assign all values to node variables
    n->index = *(int *)(data);
    data += 4;
    n->parent_index = *(int *)(data);
    data += 4;
    for (int i = 0; i < n->bounds->mbr.size(); i++)
    {
        n->bounds->mbr[i].first = *(int *)(data);
        data += 4;
    }
    for (int i = 0; i < n->bounds->mbr.size(); i++)
    {
        n->bounds->mbr[i].second = *(int *)(data);
        data += 4;
    }

    for (int i = 0; i < n->child_index.size(); i++)
    {
        n->child_index[i] = *(int *)(data);
        data += 4;
        for (int j = 0; j < d; i++)
        {
            n->child_bounds[i]->mbr[j].first = *(int *)(data);
            data += 4;
        }
        for (int j = 0; j < d; i++)
        {
            n->child_bounds[i]->mbr[j].second = *(int *)(data);
            data += 4;
        }
    }
    return n;
}

// This function is to copy data from node to the memory
void copy_data(Node *n, char *data)
{
    memcpy(data, &n->index, sizeof(int));
    data += sizeof(int);
    memcpy(data, &n->parent_index, sizeof(int));
    data += sizeof(int);
    for (int i = 0; i < n->bounds->mbr.size(); i++)
    {
        memcpy(data, &n->bounds->mbr[i].first, sizeof(int));
        data += sizeof(int);
    }
    for (int i = 0; i < n->bounds->mbr.size(); i++)
    {
        memcpy(data, &n->bounds->mbr[i].second, sizeof(int));
        data += sizeof(int);
    }
    for (int i = 0; i < n->child_index.size(); i++)
    {
        memcpy(data, &n->child_index[i], sizeof(int));
        data += sizeof(int);
        for (int j = 0; j < d; i++)
        {
            memcpy(data, &n->child_bounds[i]->mbr[j].first, sizeof(int));
            data += sizeof(int);
        }
        for (int j = 0; j < d; i++)
        {
            memcpy(data, &n->child_bounds[i]->mbr[j].second, sizeof(int));
            data += 4;
        }
    }
}

Node *get_node(char *data, int node_id)
{
    while (*((int *)(data)) != node_id)
        data += NodeSize;

    return extract_node(data);
}

void assignParents(int start, int end, FileManager &fm, FileHandler &fh)
{
    // N = Number of nodes bytes to be inserted into the tree
    int N = end - start;
    int numNodes = 0;
    int startPageNumber, endPageNumber;

    try
    {
        // startPage tells the page to look at for data
        int startPage = start / (PAGE_CONTENT_SIZE / NodeSize);

        PageHandler startPageHandler = fh.PageAt(startPage);
        PageHandler endPageHandler = fh.LastPage();

        // pointers to current data read location and last data read location
        char *startData, *endData;

        startData = startPageHandler.GetData();
        endData = endPageHandler.GetData();
        endData += endPageOffset;

        startPageNumber = startPageHandler.GetPageNum();
        endPageNumber = endPageHandler.GetPageNum();

        fh.MarkDirty(startPageNumber);
        fh.MarkDirty(endPageNumber);

        int offsetData = 0;
        while (*((int *)(startData)) != start)
        {
            startData += NodeSize;
            offsetData += NodeSize;
        }

        while (N > 0)
        {
            Node *newNode = new Node();
            newNode->index = focusIndex;
            focusIndex += 1;

            int nodeCount = 0;

            MBR *newBounds = new MBR(d);

            while (N > 0 && maxCap > nodeCount)
            {
                if (PAGE_CONTENT_SIZE - NodeSize < offsetData)
                {
                    fh.UnpinPage(startPageNumber);

                    startPageHandler = fh.NextPage(startPageNumber);
                    startPageNumber = startPageHandler.GetPageNum();
                    fh.MarkDirty(startPageNumber);

                    startData = startPageHandler.GetData();
                    offsetData = 0;
                }

                Node *copyNode = extract_node(startData);
                memcpy((startData + 4), &newNode->index, sizeof(int));

                startData = NodeSize + startData;
                offsetData = NodeSize + offsetData;

                copyNode->parent_index = newNode->index;

                newNode->child_index[nodeCount] = copyNode->index;

                for (int i = 0; i < d; i++)
                {
                    newNode->child_bounds[nodeCount]->mbr[i].first = copyNode->bounds->mbr[i].first;
                    newNode->child_bounds[nodeCount]->mbr[i].second = copyNode->bounds->mbr[i].second;

                    newBounds->mbr[i].first = min(newBounds->mbr[i].first, newNode->child_bounds[nodeCount]->mbr[i].first);
                    newBounds->mbr[i].second = min(newBounds->mbr[i].second, newNode->child_bounds[nodeCount]->mbr[i].second);
                }

                nodeCount++;
                N--;

                delete_node(copyNode);
            }

            newNode->set_left_bounds(newBounds);
            newNode->set_right_bounds(newBounds);

            if (PAGE_CONTENT_SIZE - NodeSize < endPageOffset)
            {
                fh.UnpinPage(endPageNumber);
                endPageHandler = fh.NewPage();
                endPageNumber = endPageHandler.GetPageNum();
                fh.MarkDirty(endPageNumber);

                endData = endPageHandler.GetData();
                endPageOffset = 0;
            }

            copy_data(newNode, endData);
            endData += NodeSize;
            endPageOffset += NodeSize;
            numNodes++;

            delete_node(newNode);
        }
    }
    catch (InvalidPageException)
    {
        cout << "[*] ERROR: Page is Invalid" << endl;
    }

    fh.UnpinPage(startPageNumber);
    fh.UnpinPage(endPageNumber);
    fh.FlushPages();

    if (numNodes == 1)
    {
        rootIndex = focusIndex - 1;
    }
    else if (numNodes != 1)
    {
        assignParents(end, end + numNodes, fm, fh);
    }

    return;
}

void bulkLoad(int n, FileManager &fm, FileHandler &startFileHandler, FileHandler &endFileHandler)
{
    int startPageNumber, endPageNumber;
    int N = n;

    int numNodes = 0;
    try
    {
        char *startData, *endData;

        PageHandler startPageHandler = startFileHandler.FirstPage();
        PageHandler endPageHandler = endFileHandler.NewPage();

        startPageNumber = startPageHandler.GetPageNum();
        endPageNumber = endPageHandler.GetPageNum();

        startData = startPageHandler.GetData();
        endFileHandler.MarkDirty(endPageNumber);
        endData = endPageHandler.GetData();

        int pageOffset = 0;
        while (N > 0)
        {
            Node *newNode = new Node();
            newNode->index = focusIndex;
            focusIndex += 1;

            int nodeCount = 0;

            MBR *newBounds = new MBR(d);

            while (N > 0 && maxCap > nodeCount)
            {
                if (PAGE_CONTENT_SIZE - (d * sizeof(int)) < pageOffset)
                {
                    startFileHandler.UnpinPage(startPageNumber);
                    startPageHandler = startFileHandler.NextPage(startPageNumber);

                    startData = startPageHandler.GetData();
                    startPageNumber = startPageHandler.GetPageNum();

                    pageOffset = 0;
                }

                newNode->child_index[nodeCount] = -1;
                for (int i = 0; i < d; i++)
                {
                    newNode->child_bounds[nodeCount]->mbr[i].first = *(int *)(startData);
                    newNode->child_bounds[nodeCount]->mbr[i].second = INT_MIN;
                    startData += sizeof(int);
                    pageOffset += sizeof(int);

                    newBounds->mbr[i].first = min(newBounds->mbr[i].first, newNode->child_bounds[nodeCount]->mbr[i].first);
                    newBounds->mbr[i].second = max(newBounds->mbr[i].second, newNode->child_bounds[nodeCount]->mbr[i].second);
                }

                nodeCount++;
                N--;
            }

            newNode->set_left_bounds(newBounds);
            newNode->set_right_bounds(newBounds);

            if (PAGE_CONTENT_SIZE - NodeSize < endPageOffset)
            {
                endFileHandler.UnpinPage(endPageNumber);

                endPageHandler = endFileHandler.NewPage();
                endPageNumber = endPageHandler.GetPageNum();

                endFileHandler.MarkDirty(endPageNumber);

                endData = endPageHandler.GetData();
                endPageOffset = 0;
            }

            copy_data(newNode, endData);
            endData += NodeSize;

            endPageOffset += NodeSize;
            numNodes++;

            delete_node(newNode);
        }
    }
    catch (InvalidPageException)
    {
        cout << "[*] Error: Page is Invalid." << endl;
    }

    startFileHandler.UnpinPage(startPageNumber);
    endFileHandler.UnpinPage(endPageNumber);

    startFileHandler.FlushPages();
    endFileHandler.FlushPages();

    assignParents(0, numNodes, fm, endFileHandler);
}

// merge_mbr
MBR *new_area_mbr(MBR *a, MBR *b)
{
    MBR *new_mbr = new MBR();
    for (int i = 0; i < a->mbr.size(); i++)
    {
        new_mbr->mbr[i].first = min(a->mbr[i].first, b->mbr[i].first);
        new_mbr->mbr[i].second = max(a->mbr[i].second, b->mbr[i].second);
    }
    return new_mbr;
}

// convert a single point to mbr
MBR *point_mbr(vector<int> v)
{
    MBR *m = new MBR();
    for (int i = 0; i < v.size(); i++)
    {
        m->mbr[i].first = v[i];
        m->mbr[i].second = v[i];
    }
    return m;
}

MBR *new_area_point(MBR *a, vector<int> p)
{
    MBR *b = point_mbr(p);
    MBR *new_mbr = new_area_mbr(a, b);
    delete_mbr(b);
    return new_mbr;
}
// hyper_area
long int get_area(MBR *m)
{
    long int area = 1;
    for (int i = 0; i < m->mbr.size(); i++)
    {
        area *= (m->mbr[i].second - m->mbr[i].first);
    }
    return area;
}
// check if a belongs to the region of b
bool belongs(MBR *a, MBR *b)
{
    for (int i = 0; i < a->mbr[i].size(); i++)
    {
        if (a->mbr[i].first < b->mbr[i].first || a->mbr[i].second > b->mbr[i].second)
            return false;
    }
    return true;
}

// check if two mbr's are same
bool is_equal(MBR *a, MBR *b)
{
    for (int i = 0; i < a->mbr.size(); i++)
    {
        if (!(a->mbr[i].first == b->mbr[i].first && a->mbr[i].second == b->mbr[i].second))
            return false;
    }
    return true;
}

bool search(vector<int> &P, FileHandler &fh, int index)
{
    int page_id = index / (PAGE_CONTENT_SIZE / NODE_SIZE);
    PageHandler ph = fh.Pageat(page_id);
    char *data = ph.GetData();
    Node *n = get_node(data, index);

    if (node->is_leaf())
    {
        for (int i = 0; i < maxCap; i++)
        {
            MBR *m = point_mbr(P);
            if (is_equal(m, n->child_bounds[i]))
            {
                delete_mbr(m);
                delete_node(n);
                fh.UnpinPage(ph.GetPageNum());
                return true;
            }
            delete_mbr(m);
        }
        delete_node(n);
        fh.UnpinPage(ph.GetPageNum());
        return false;
    }
    else
    {
        for (int i = 0; i < maxCap; i++)
        {
            if (n->child_index[i] == INT_MIN)
                break;

            MBR *m = point_mbr(P);
            if (belongs(m, n))
            {
                if (search(P, fh, n->child_index[i]))
                {
                    delete_mbr(m);
                    delete_node(n);
                    fh.UnpinPage(ph.GetPageNum());
                    return true;
                }
            }
            delete_mbr(m);
        }
    }
    delete_node(n);
    fh.UnpinPage(ph.GetPageNum());
    return false;
}

int main()
{
    cout << "None" << endl;
}
