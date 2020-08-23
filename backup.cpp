#include "file_manager.h"
#include "buffer_manager.h"
#include "constants.h"
#include "errors.h"

#include <bits/stdc++.h>
#include <iostream>
#include <fstream>
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
    vector<int> childIndex;
    vector<MBR *> child_bounds;

    Node()
    {
        this->index = -1;
        this->parent_index = -1;
        this->bounds = new MBR(d);
        this->childIndex.resize(maxCap, INT_MIN);
        this->child_bounds.resize(maxCap);

        for (int i = 0; i < child_bounds.size(); i++)
        {
            this->child_bounds[i] = new MBR(d);
        }
    }

    void setBounds(MBR *m)
    {
        this->bounds = m;
    }

    void setLeftBounds(MBR *m)
    {
        for (int i = 0; i < m->mbr.size(); i++)
        {
            this->bounds->mbr[i].first = m->mbr[i].first;
        }
    }

    void setRightBounds(MBR *m)
    {
        for (int i = 0; i < m->mbr.size(); i++)
        {
            this->bounds->mbr[i].second = m->mbr[i].second;
        }
    }

    bool isLeaf()
    {
        return (this->childIndex[0] == -1);
    }
};

// store node in char* data
Node *extract_node(char *data)
{
    Node *n = new Node();
    // assign all values to node variables
    n->index = *(int *)(data);
    data += sizeof(int);
    n->parent_index = *(int *)(data);
    data += sizeof(int);
    for (int i = 0; i < n->bounds->mbr.size(); i++)
    {
        n->bounds->mbr[i].first = *(int *)(data);
        data += sizeof(int);
    }
    for (int i = 0; i < n->bounds->mbr.size(); i++)
    {
        n->bounds->mbr[i].second = *(int *)(data);
        data += sizeof(int);
    }

    for (int i = 0; i < n->childIndex.size(); i++)
    {
        n->childIndex[i] = *(int *)(data);
        data += sizeof(int);
        for (int j = 0; j < d; j++)
        {
            n->child_bounds[i]->mbr[j].first = *(int *)(data);
            data += sizeof(int);
        }
        for (int j = 0; j < d; j++)
        {
            n->child_bounds[i]->mbr[j].second = *(int *)(data);
            data += sizeof(int);
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
    int i = 0;
    while (i < n->childIndex.size())
    {
        memcpy(data, &n->childIndex[i], sizeof(int));
        data += sizeof(int);
        for (int j = 0; j < d; j++)
        {
            memcpy(data, &n->child_bounds[i]->mbr[j].first, sizeof(int));
            data += sizeof(int);
        }
        for (int j = 0; j < d; j++)
        {
            memcpy(data, &n->child_bounds[i]->mbr[j].second, sizeof(int));
            data += sizeof(int);
        }
        i++;
    }
}

Node *getNode(char *data, int node_id)
{
    while (*((int *)(data)) != node_id)
        data += NodeSize;

    return extract_node(data);
}

void deleteBounds(MBR *m)
{
    m->mbr.clear();
    m->mbr.shrink_to_fit();
    delete (m);
}

void deleteNode(Node *n)
{
    deleteBounds(n->bounds);
    n->childIndex.clear();
    n->childIndex.shrink_to_fit();
    for (int i = 0; i < n->child_bounds.size(); i++)
    {
        deleteBounds(n->child_bounds[i]);
    }
    n->child_bounds.clear();
    n->child_bounds.shrink_to_fit();
    delete (n);
}

void assignParents(int start, int end, FileManager &fm, FileHandler &fh)
{
    // N = Number of nodes bytes to be inserted into the tree
    int startPageNumber, endPageNumber;
    int numNodes = 0;
    int N = end - start;
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

        // char *startData, *endData;
        // PageHandler startPageHandler = fh.PageAt(startPage);
        // startData = startPageHandler.GetData();
        // startPageNumber = startPageHandler.GetPageNum();
        // fh.MarkDirty(startPageNumber);

        // PageHandler endPageHandler = fh.LastPage();
        // endPageNumber = endPageHandler.GetPageNum();
        // fh.MarkDirty(endPageNumber);

        // endData = endPageHandler.GetData();
        // endData = endData + endPageOffset;

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
            for (int i = 0; i < d; i++)
            {
                newBounds->mbr[i].first = INT_MAX;
                newBounds->mbr[i].second = INT_MIN;
            }

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
                memcpy((startData + sizeof(int)), &newNode->index, sizeof(int));

                copyNode->parent_index = newNode->index;

                newNode->childIndex[nodeCount] = copyNode->index;

                startData = NodeSize + startData;
                offsetData = NodeSize + offsetData;
                for (int i = 0; i < d; i++)
                {
                    newNode->child_bounds[nodeCount]->mbr[i].first = copyNode->bounds->mbr[i].first;
                    newNode->child_bounds[nodeCount]->mbr[i].second = copyNode->bounds->mbr[i].second;

                    newBounds->mbr[i].first = min(newBounds->mbr[i].first, newNode->child_bounds[nodeCount]->mbr[i].first);
                    newBounds->mbr[i].second = max(newBounds->mbr[i].second, newNode->child_bounds[nodeCount]->mbr[i].second);
                }
                N--;
                nodeCount++;
                deleteNode(copyNode);
            }
            newNode->setBounds(newBounds);

            if (PAGE_CONTENT_SIZE - NodeSize < endPageOffset)
            {
                fh.UnpinPage(endPageNumber);
                endPageHandler = fh.NewPage();
                endPageNumber = endPageHandler.GetPageNum();
                fh.MarkDirty(endPageNumber);
                endData = endPageHandler.GetData();
                endPageOffset = 0;
            }
            numNodes += 1;
            copy_data(newNode, endData);
            deleteNode(newNode);
            endData += NodeSize;
            endPageOffset += NodeSize;
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
        rootIndex = focusIndex - 1;
    else if (numNodes != 1)
        assignParents(end, end + numNodes, fm, fh);

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
            for (int i = 0; i < d; i++)
            {
                newBounds->mbr[i].first = INT_MAX;
                newBounds->mbr[i].second = INT_MIN;
            }
            while (N > 0 && maxCap > nodeCount)
            {
                if (PAGE_CONTENT_SIZE - (d * sizeof(int)) < pageOffset)
                {
                    pageOffset = 0;
                    startFileHandler.UnpinPage(startPageNumber);
                    startPageHandler = startFileHandler.NextPage(startPageNumber);
                    startPageNumber = startPageHandler.GetPageNum();
                    startData = startPageHandler.GetData();
                }

                newNode->childIndex[nodeCount] = -1;
                for (int i = 0; i < d; i++)
                {
                    newNode->child_bounds[nodeCount]->mbr[i].first = *(int *)(startData);
                    newNode->child_bounds[nodeCount]->mbr[i].second = INT_MIN;
                    startData += sizeof(int);
                    pageOffset += sizeof(int);
                    newBounds->mbr[i].first = min(newBounds->mbr[i].first, newNode->child_bounds[nodeCount]->mbr[i].first);
                    newBounds->mbr[i].second = max(newBounds->mbr[i].second, newNode->child_bounds[nodeCount]->mbr[i].first);
                }
                nodeCount++;
                N--;
            }

            newNode->setBounds(newBounds);
            // for(int i=0;i<d;i++){
            //     cerr << "Go check 2nd value " << newNode->bounds->mbr[i].second << endl;
            // }
            // check if mbr value is correctly set
            for (int i = 0; i < d; i++)
            {
                if (newBounds->mbr[i].first != newNode->bounds->mbr[i].first || newBounds->mbr[i].second != newNode->bounds->mbr[i].second)
                    assert(0);
            }
            // newNode->setLeftBounds(newBounds);
            // newNode->setRightBounds(newBounds);

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
            deleteNode(newNode);
            numNodes += 1;
            endData += NodeSize;
            endPageOffset += NodeSize;
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
    for (int i = 0; i < m->mbr.size(); i++)
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
    deleteBounds(b);
    return new_mbr;
}
// hyper_area
long int get_area(MBR *m)
{
    long int area = 1;
    for (int i = 0; i < d; i++)
    {
        area *= (m->mbr[i].second - m->mbr[i].first);
    }
    return area;
}
// check if a belongs to the region of b
bool belongs(MBR *a, MBR *b)
{
    for (int i = 0; i < a->mbr.size(); i++)
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

long long int boundsHyperVolume(MBR *a)
{
    long long int hyperVolume = 1;
    for (int dim = 0; dim < d; dim++)
    {
        hyperVolume *= (a->mbr[dim].second - a->mbr[dim].first);
    }
    return hyperVolume;
}

int getLeafIndex(FileHandler &fileHandler, vector<int> &Point, int nodeIndex)
{
    PageHandler pageHandler = fileHandler.PageAt((nodeIndex / (PAGE_CONTENT_SIZE / NodeSize)));
    char *data = pageHandler.GetData();

    Node *leafNode = getNode(data, nodeIndex);

    if (leafNode->childIndex[0] != -1)
    {
        int childIndexStore = -1;
        int minimumVal = INT_MAX;
        for (int i = 0; i < maxCap; i++)
        {
            if (leafNode->childIndex[i] != INT_MIN)
            {
                long long int nodeVolume = boundsHyperVolume(leafNode->child_bounds[i]);
                long long int val = boundsHyperVolume(new_area_point(leafNode->child_bounds[i], Point)) - nodeVolume;

                if (minimumVal > val)
                {
                    minimumVal = val;
                    childIndexStore = i;
                }
                else if (minimumVal == val)
                {
                    long long int nodeVolumeSecond = boundsHyperVolume(leafNode->child_bounds[childIndexStore]);
                    if (nodeVolumeSecond > nodeVolume)
                    {
                        childIndexStore = i;
                    }
                }
            }
            else
            {
                continue;
            }
        }

        fileHandler.UnpinPage(pageHandler.GetPageNum());
        return (getLeafIndex(fileHandler, Point, leafNode->childIndex[childIndexStore]));
    }
    else
    {
        return (nodeIndex);
    }
}

void print_mbr(MBR *m)
{
    cout << "Printing MBR \n";
    for (int i = 0; i < m->mbr.size(); i++)
    {
        cout << m->mbr[i].first << " " << m->mbr[i].second << "\n";
    }
    return;
}

bool search(vector<int> &P, FileHandler &fh, int index)
{
    int page_id = index / (PAGE_CONTENT_SIZE / NodeSize);
    PageHandler ph = fh.PageAt(page_id);
    char *data = ph.GetData();
    Node *n = getNode(data, index);

    if (n->isLeaf())
    {
        for (int i = 0; i < maxCap; i++)
        {
            MBR *m = point_mbr(P);
            int is_same = 1;
            for (int j = 0; j < P.size(); j++)
            {
                if ((P[j] != n->child_bounds[i]->mbr[j].first))
                {
                    is_same = 0;
                    break;
                }
            }
            if (is_same == 1)
            {
                deleteBounds(m);
                deleteNode(n);
                fh.UnpinPage(ph.GetPageNum());
                return true;
            }
            deleteBounds(m);
        }
        deleteNode(n);
        fh.UnpinPage(ph.GetPageNum());
        return false;
    }
    else
    {
        for (int i = 0; i < maxCap; i++)
        {
            if (n->childIndex[i] == INT_MIN)
            {
                break;
            }

            MBR *m = point_mbr(P);
            int is_belong = 1;
            for (int k = 0; k < m->mbr.size(); k++)
            {
                if ((m->mbr[k].first < n->child_bounds[i]->mbr[k].first) || (m->mbr[k].first > n->child_bounds[i]->mbr[k].second))
                {
                    is_belong = 0;
                    break;
                }
            }
            if (is_belong == 1)
            {
                if (search(P, fh, n->childIndex[i]))
                {
                    deleteBounds(m);
                    deleteNode(n);
                    fh.UnpinPage(ph.GetPageNum());
                    return true;
                }
            }
            deleteBounds(m);
        }
    }
    deleteNode(n);
    fh.UnpinPage(ph.GetPageNum());
    return false;
}

void updateMBR(FileHandler &fileHandler, int nodeParentIndex, MBR *newBounds, int nodeIndex, bool split)
{
    if (nodeParentIndex != -1)
    {
        PageHandler pageHandler = fileHandler.PageAt(nodeParentIndex / (PAGE_CONTENT_SIZE / NodeSize));
        fileHandler.MarkDirty(pageHandler.GetPageNum());
        char *data = pageHandler.GetData();

        while (*(int *)(data) != nodeParentIndex)
        {
            data += NodeSize;
        }

        Node *parentNode = getNode(data, nodeParentIndex);

        MBR *parentBounds = new_area_mbr(parentNode->bounds, newBounds);
        data += 8;

        for (int i = 0; i < d; i++)
        {
            memcpy(data, &parentBounds->mbr[i].first, sizeof(int));
            data += sizeof(int);
        }
        for (int i = 0; i < d; i++)
        {
            memcpy(data, &parentBounds->mbr[i].second, sizeof(int));
            data += sizeof(int);
        }

        if (split == true)
        {
            while (*(int *)(data) != nodeIndex)
            {
                data += (4 + 8 * d);
            }
            data += 4;

            for (int j = 0; j < 2 * d; j++)
            {
                memcpy(data, &newBounds[j], 4);
                data += 4;
            }
        }
        else
        {
            while (*(int *)(data) != INT_MIN)
            {
                data += (4 + 8 * d);
            }
            memcpy(data, &nodeIndex, 4);
            data += 4;

            for (int j = 0; j < 2 * d; j++)
            {
                memcpy(data, &newBounds[j], 4);
                data += 4;
            }
        }

        fileHandler.UnpinPage(pageHandler.GetPageNum());
        fileHandler.FlushPage(pageHandler.GetPageNum());

        updateMBR(fileHandler, nodeParentIndex, parentBounds, parentNode->index, true);
    }
    else
    {
        return;
    }
}

void addSplitNode(FileHandler &fileHandler, Node *node)
{
    PageHandler pageHandler = fileHandler.LastPage();

    int p1 = (node->index / (PAGE_CONTENT_SIZE / NodeSize));
    int p2 = pageHandler.GetPageNum();

    if (p1 == p2 + 1)
    {
        fileHandler.UnpinPage(p2);
        fileHandler.UnpinPage(p2);

        pageHandler = fileHandler.NewPage();
        endPageOffset = 0;

        char *data = pageHandler.GetData();
        copy_data(node, data);
    }
    else
    {
        fileHandler.MarkDirty(pageHandler.GetPageNum());
        char *data = pageHandler.GetData();

        while (*(int *)(data) != node->index - 1)
        {
            data += NodeSize;
        }

        data += NodeSize;
        copy_data(node, data);
    }

    fileHandler.UnpinPage(pageHandler.GetPageNum());
}

long long int eucledianDistance(MBR *a, MBR *b)
{
    long long int distance = 0;
    for (int i = 0; i < a->mbr.size(); i++)
    {
        distance += (a->mbr[i].first - b->mbr[i].first) * (a->mbr[i].first - b->mbr[i].first);
        distance += (a->mbr[i].second - b->mbr[i].second) * (a->mbr[i].second - b->mbr[i].second);
    }
    return distance;
}

Node *splitLeafNode(FileHandler &fileHandler, char *data, int nodeIndex, MBR *a)
{
    Node *newNode = getNode(data, nodeIndex);
    int orignalIndex = newNode->index;
    vector<MBR *> newChildBounds = newNode->child_bounds;

    a->mbr.resize(d, make_pair(INT_MIN, INT_MIN));
    newChildBounds.push_back(a);

    int newParentIndex = newNode->parent_index;
    int n1, n2 = -1;
    MBR *nb1, *nb2;
    vector<int> nc1, nc2;
    long long int diagonal = LONG_MIN;

    for (int i = 0; i < newChildBounds.size(); i++)
    {
        for (int j = i + 1; j < newChildBounds.size(); j++)
        {
            long long distance = eucledianDistance(newChildBounds[i], newChildBounds[j]);
            if (diagonal < distance)
            {
                n1 = i;
                n2 = j;
                diagonal = distance;
            }
        }
    }

    for (int i = 0; i < d; i++)
    {
        nb1->mbr.push_back(newChildBounds[n1]->mbr[i]);
        nb2->mbr.push_back(newChildBounds[n2]->mbr[i]);
    }
    for (int i = 0; i < d; i++)
    {
        nb1->mbr.push_back(nb1->mbr[i]);
        nb2->mbr.push_back(nb2->mbr[i]);
    }

    int nodeCount = 0;
    for (int i = 0; i < newChildBounds.size(); i++)
    {
        long long int hyperVol1 = boundsHyperVolume(nb1);
        long long int hyperVol2 = boundsHyperVolume(nb2);

        MBR *b1 = new_area_mbr(nb1, newChildBounds[i]);
        MBR *b2 = new_area_mbr(nb2, newChildBounds[i]);

        long long int newHyperVol1 = boundsHyperVolume(b1) - hyperVol1;
        long long int newHyperVol2 = boundsHyperVolume(b2) - hyperVol2;

        nodeCount += 1;
        if (newHyperVol1 > newHyperVol2)
        {
            nc2.push_back(i);
            nb2 = b2;
        }
        else if (newHyperVol1 < newHyperVol2)
        {
            nc1.push_back(i);
            nb1 = b1;
        }
        else
        {
            if (hyperVol1 < hyperVol2)
            {
                nc1.push_back(i);
                nb1 = b1;
            }
            else if (hyperVol1 > hyperVol2)
            {
                nc2.push_back(i);
                nb2 = b2;
            }
            else
            {
                if (nc1.size() < nc2.size())
                {
                    nc1.push_back(i);
                    nb1 = b1;
                }
                else
                {
                    nc2.push_back(i);
                    nb2 = b2;
                }
            }
        }

        if (newChildBounds.size() - nodeCount == int(ceil((float)maxCap / 2.0)) - nc1.size())
        {
            for (int j = i + 1; j < newChildBounds.size(); j++)
            {
                MBR *b1 = new_area_mbr(nb1, newChildBounds[j]);
                nc1.push_back(j);
                nb1 = b1;
            }
            break;
        }
        if (newChildBounds.size() - nodeCount == int(ceil((float)maxCap / 2.0)) - nc2.size())
        {
            for (int j = i + 1; j < newChildBounds.size(); j++)
            {
                MBR *b2 = new_area_mbr(nb2, newChildBounds[j]);
                nc2.push_back(j);
                nb2 = b2;
            }
            break;
        }
    }
    newNode->bounds = nb1;

    for (int i = 0; i < maxCap; i++)
    {
        if (nc1.size() > i)
        {
            newNode->child_bounds[i] = newChildBounds[nc1[i]];
            newNode->childIndex[i] = -1;
        }
        else
        {
            newNode->child_bounds[i] = new MBR(d);
            newNode->childIndex[i] = INT_MIN;
        }
    }

    Node *newerNode = new Node();
    newerNode->bounds = nb2;
    newerNode->index = focusIndex++;
    newerNode->parent_index = newParentIndex;

    for (int i = 0; i < maxCap; i++)
    {
        if (i < nc2.size())
        {
            newerNode->child_bounds[i] = newChildBounds[nc2[i]];
            newerNode->childIndex[i] = -1;
        }
        else
        {
            newNode->child_bounds[i] = new MBR(d);
            newNode->childIndex[i] = INT_MIN;
        }
    }

    if (newParentIndex == -1)
    {
        Node *rootNode = new Node();

        rootNode->index = focusIndex++;
        rootNode->parent_index = -1;
        rootIndex = rootNode->index;

        MBR *bnds = new_area_mbr(nb1, nb2);

        rootNode->bounds = bnds;
        rootNode->childIndex[0] = newNode->index;
        rootNode->child_bounds[0] = newNode->bounds;

        newerNode->parent_index = rootIndex;
        newNode->parent_index = rootIndex;

        addSplitNode(fileHandler, newerNode);

        while (*(int *)(data) != newNode->index)
        {
            data += NodeSize;
        }
        copy_data(newNode, data);

        addSplitNode(fileHandler, rootNode);
        deleteNode(rootNode);
        return (newerNode);
    }

    while (*(int *)(data) != newNode->index)
    {
        data += NodeSize;
    }
    copy_data(newNode, data);

    updateMBR(fileHandler, newNode->parent_index, newNode->bounds, newNode->index, true);
    addSplitNode(fileHandler, newerNode);
    deleteNode(newNode);
    return (newerNode);
}

Node *splitInternal(FileHandler &fileHandler, char *data, int nodeIndex, Node *node)
{
    Node *newNode = getNode(data, nodeIndex);
    vector<MBR *> newChildBounds = newNode->child_bounds;
    vector<int> newchildIndex = newNode->childIndex;

    int orignalIndex = newNode->index;
    int parentIndex = newNode->index;

    long long int diagonal = LONG_MIN;
    int n1, n2 = -1;
    vector<int> nc1, nc2;
    MBR *nb1, *nb2;

    newChildBounds.push_back(node->bounds);
    newchildIndex.push_back(node->index);

    for (int i = 0; i < newChildBounds.size(); i++)
    {
        for (int j = i + 1; j < newChildBounds.size(); j++)
        {
            long long distance = eucledianDistance(newChildBounds[i], newChildBounds[j]);
            if (diagonal < distance)
            {
                n1 = i;
                n2 = j;
                diagonal = distance;
            }
        }
    }

    nb1 = newChildBounds[n1];
    nb2 = newChildBounds[n2];

    int nodeCount = 0;
    for (int i = 0; i < newChildBounds.size(); i++)
    {
        long long int hyperVol1 = boundsHyperVolume(nb1);
        long long int hyperVol2 = boundsHyperVolume(nb2);

        MBR *b1 = new_area_mbr(nb1, newChildBounds[i]);
        MBR *b2 = new_area_mbr(nb2, newChildBounds[i]);

        long long int newHyperVol1 = boundsHyperVolume(b1) - hyperVol1;
        long long int newHyperVol2 = boundsHyperVolume(b2) - hyperVol2;

        nodeCount += 1;

        if (newHyperVol1 > newHyperVol2)
        {
            nc2.push_back(i);
            nb2 = b2;
        }
        else if (newHyperVol1 < newHyperVol2)
        {
            nc1.push_back(i);
            nb1 = b1;
        }
        else
        {
            if (hyperVol1 < hyperVol2)
            {
                nc1.push_back(i);
                nb1 = b1;
            }
            else if (hyperVol1 > hyperVol2)
            {
                nc2.push_back(i);
                nb2 = b2;
            }
            else
            {
                if (nc1.size() < nc2.size())
                {
                    nc1.push_back(i);
                    nb1 = b1;
                }
                else
                {
                    nc2.push_back(i);
                    nb2 = b2;
                }
            }
        }

        if (newChildBounds.size() - nodeCount == int(ceil((float)maxCap / 2.0)) - nc1.size())
        {
            for (int j = i + 1; j < newChildBounds.size(); j++)
            {
                MBR *b1 = new_area_mbr(nb1, newChildBounds[j]);
                nc1.push_back(j);
                nb1 = b1;
            }
            break;
        }
        if (newChildBounds.size() - nodeCount == int(ceil((float)maxCap / 2.0)) - nc2.size())
        {
            for (int j = i + 1; j < newChildBounds.size(); j++)
            {
                MBR *b2 = new_area_mbr(nb2, newChildBounds[j]);
                nc2.push_back(j);
                nb2 = b2;
            }
            break;
        }
    }

    newNode->bounds = nb1;

    for (int i = 0; i < maxCap; i++)
    {
        if (nc1.size() > i)
        {
            newNode->child_bounds[i] = newChildBounds[nc1[i]];
            newNode->childIndex[i] = newchildIndex[nc1[i]];
        }
        else
        {
            newNode->child_bounds[i] = new MBR(d);
            newNode->childIndex[i] = INT_MIN;
        }
    }

    Node *newerNode = new Node();
    newerNode->bounds = nb2;
    newerNode->index = focusIndex++;
    newerNode->parent_index = parentIndex;

    for (int i = 0; i < maxCap; i++)
    {
        if (i < nc2.size())
        {
            newerNode->child_bounds[i] = newChildBounds[nc2[i]];
            newerNode->childIndex[i] = newchildIndex[nc2[i]];
        }
        else
        {
            newNode->child_bounds[i] = new MBR(d);
            newNode->childIndex[i] = INT_MIN;
        }
    }

    // Updating Parents
    for (int i = 0; i < nc2.size(); i++)
    {
        int indexUpdate = newchildIndex[nc2[i]];
        PageHandler pageHandler = fileHandler.PageAt(indexUpdate / (PAGE_CONTENT_SIZE / NodeSize));
        fileHandler.MarkDirty(pageHandler.GetPageNum());

        char *data = pageHandler.GetData();

        while (*(int *)(data) != indexUpdate)
            data += NodeSize;
        data += sizeof(int);
        memcpy(data, &(newerNode->index), sizeof(int));

        fileHandler.UnpinPage(pageHandler.GetPageNum());
    }

    if (parentIndex == -1)
    {
        Node *rootNode = new Node();

        rootNode->index = focusIndex++;
        rootNode->parent_index = -1;
        rootIndex = rootNode->index;

        MBR *bnds = new_area_mbr(nb1, nb2);

        rootNode->bounds = bnds;
        rootNode->childIndex[0] = newNode->index;
        rootNode->child_bounds[0] = newNode->bounds;

        newerNode->parent_index = rootIndex;
        newNode->parent_index = rootIndex;

        addSplitNode(fileHandler, newerNode);

        while (*(int *)(data) != newNode->index)
        {
            data += NodeSize;
        }
        copy_data(newNode, data);

        addSplitNode(fileHandler, rootNode);
        deleteNode(rootNode);
        return (newerNode);
    }

    while (*(int *)(data) != newNode->index)
    {
        data += NodeSize;
    }
    copy_data(newNode, data);

    updateMBR(fileHandler, newNode->parent_index, newNode->bounds, newNode->index, true);
    addSplitNode(fileHandler, newerNode);

    deleteNode(newNode);
    return (newerNode);
}

void updateNode(FileHandler &fileHandler, int nodeParentIndex, Node *node)
{
    PageHandler pageHandler = fileHandler.PageAt(nodeParentIndex / (PAGE_CONTENT_SIZE / NodeSize));
    fileHandler.MarkDirty(pageHandler.GetPageNum());
    char *data = pageHandler.GetData();

    Node *nodeParent = getNode(data, nodeParentIndex);
    MBR *newBounds = node->bounds;

    if (nodeParent->childIndex[maxCap - 1] == INT_MIN)
    {
        updateMBR(fileHandler, nodeParentIndex, newBounds, node->index, false);
        return;
    }
    else
    {
        Node *newNode = splitInternal(fileHandler, data, nodeParentIndex, node);
        updateNode(fileHandler, newNode->index, newNode);

        deleteNode(newNode);
    }

    deleteNode(nodeParent);
    fileHandler.UnpinPage(pageHandler.GetPageNum());
}

void insertNode(FileHandler &fileHandler, FileManager &fm, vector<int> &Point, int nodeIndex)
{
    int leafIndex = getLeafIndex(fileHandler, Point, nodeIndex);
    fileHandler.FlushPages();

    PageHandler PageHandler = fileHandler.PageAt(leafIndex / (PAGE_CONTENT_SIZE / NodeSize));

    fileHandler.MarkDirty(PageHandler.GetPageNum());

    char *data = PageHandler.GetData();
    Node *newNode = getNode(data, leafIndex);

    if (newNode->childIndex[maxCap - 1] == INT_MIN)
    {
        MBR *newBounds = new_area_point(newNode->bounds, Point);

        int num = -1;

        while (*(int *)(data) != leafIndex)
        {
            data += NodeSize;
        }

        data += 8;

        for (int i = 0; i < d; i++)
        {
            num = newBounds->mbr[i].first;
            memcpy(data, &num, 4);
            data += 4;
        }
        for (int i = 0; i < d; i++)
        {
            num = newBounds->mbr[i].second;
            memcpy(data, &num, 4);
            data += 4;
        }

        while (*(int *)(data) != INT_MIN)
        {
            data += (sizeof(int) + sizeof(int) * 2 * d);
        }

        memcpy(data, &num, 4);
        data += 4;

        for (int i = 0; i < d; i++)
        {
            memcpy(data, &Point[i], 4);
            data += 4;
        }

        fileHandler.UnpinPage(PageHandler.GetPageNum());
        fileHandler.FlushPage(PageHandler.GetPageNum());

        updateMBR(fileHandler, newNode->parent_index, newBounds, leafIndex, true);
    }
    else
    {
        Node *newerNode = splitLeafNode(fileHandler, data, leafIndex, point_mbr(Point));

        fileHandler.UnpinPage(PageHandler.GetPageNum());
        fileHandler.FlushPage(PageHandler.GetPageNum());

        updateNode(fileHandler, newerNode->parent_index, newerNode);
        delete (newerNode);
    }

    fileHandler.FlushPages();
}

int main(int argc, char *argv[])
{
    FileManager fm;
    FileHandler startFileHandler, endFileHandler;
    string input;
    ifstream infile(argv[1]);
    d = stoi(argv[3]);
    maxCap = stoi(argv[2]);
    NodeSize = (1 + 1 + 2 * d + maxCap + 2 * d * maxCap) * sizeof(int);

    try
    {
        endFileHandler = fm.CreateFile("answer.txt");
    }
    catch (InvalidPageException)
    {
        fm.DestroyFile("answer.txt");
        endFileHandler = fm.CreateFile("answer.txt");
    }

    ofstream myfile;
    myfile.open(argv[4]);
    // NOTE : sizeof(pair<int,int>) = sizeof(int)*2

    while (getline(infile, input))
    {
        int position = input.find(" ");
        string command = input.substr(0, position);
        input.erase(0, position + 1);

        if (command == "BULKLOAD")
        {
            position = input.find(" ");
            string inputfile = input.substr(0, position);
            startFileHandler = fm.OpenFile(inputfile.c_str());
            input.erase(0, 1 + position);

            position = input.find(" ");
            int N = stoi(input.substr(0, position));
            input.erase(0, 1 + position);

            bulkLoad(N, fm, startFileHandler, endFileHandler);

            myfile << "BULKLOAD\n\n\n";

            fm.CloseFile(startFileHandler);
        }
        else if (command == "INSERT")
        {
            vector<int> Point;
            for (int i = 0; i < d; i++)
            {
                position = input.find(" ");
                Point.push_back(stoi(input.substr(0, position)));
                input.erase(0, 1 + position);
            }

            if (rootIndex == -1)
            {
                PageHandler pageHandler = endFileHandler.NewPage();

                char *data = pageHandler.GetData();
                Node *newNode = new Node();

                newNode->childIndex[0] = -1;
                newNode->index = focusIndex++;
                newNode->parent_index = -1;

                for (int i = 0; i < d; i++)
                {
                    newNode->bounds->mbr[i].first = Point[i];
                    newNode->bounds->mbr[i].second = Point[i];
                    newNode->child_bounds[0]->mbr[i].first = Point[i];
                }
                rootIndex = 0;

                copy_data(newNode, data);
                endFileHandler.UnpinPage(pageHandler.GetPageNum());
                endFileHandler.FlushPage(pageHandler.GetPageNum());
            }
            else
            {
                insertNode(endFileHandler, fm, Point, rootIndex);
            }

            myfile << "INSERT\n\n\n";

            endFileHandler.FlushPages();
        }
        else if (command == "QUERY")
        {
            vector<int> searchPoint;
            for (int i = 0; i < d; i++)
            {
                position = input.find(" ");
                searchPoint.push_back(stoi(input.substr(0, position)));
                input.erase(0, 1 + position);
            }
            if (search(searchPoint, endFileHandler, rootIndex))
            {
                myfile << "TRUE\n\n"
                       << endl;
            }
            else
            {
                myfile << "FALSE\n\n"
                       << endl;
            }

            endFileHandler.FlushPages();
            // myfile << endl;
        }
    }

    fm.CloseFile(endFileHandler);
    fm.DestroyFile("answer.txt");

    infile.close();
    myfile.close();

    return (0);
}