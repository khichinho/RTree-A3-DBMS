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
    int parentIndex;
    MBR *bounds;
    vector<int> childIndices;
    vector<MBR *> childBounds;

    Node()
    {
        this->index = -1;
        this->parentIndex = -1;
        this->bounds = new MBR(d);
        this->childIndices.resize(maxCap, INT_MIN);
        this->childBounds.resize(maxCap);

        for (int i = 0; i < childBounds.size(); i++)
        {
            this->childBounds[i] = new MBR(d);
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
        return (this->childIndices[0] == -1);
    }
};

// store node in char* data
Node *extractNodeData(char *data)
{
    Node *node = new Node();

    // assign all values to node variables
    node->index = *(int *)(data);
    data += 4;

    // assign all parent_indexes to node variables
    node->parentIndex = *(int *)(data);
    data += 4;
    for (int i = 0; i < node->bounds->mbr.size(); i++)
    {
        node->bounds->mbr[i].first = *(int *)(data);
        data += 4;
    }
    for (int i = 0; i < node->bounds->mbr.size(); i++)
    {
        node->bounds->mbr[i].second = *(int *)(data);
        data += 4;
    }

    for (int i = 0; i < node->childIndices.size(); i++)
    {
        node->childIndices[i] = *(int *)(data);
        data += 4;
        for (int j = 0; j < d; j++)
        {
            node->childBounds[i]->mbr[j].first = *(int *)(data);
            data += 4;
        }
        for (int j = 0; j < d; j++)
        {
            node->childBounds[i]->mbr[j].second = *(int *)(data);
            data += 4;
        }
    }
    return node;
}

// This function is to copy data from node to the memory
void copyNodeData(Node *node, char *data)
{
    memcpy(data, &node->index, 4);
    data += 4;
    memcpy(data, &node->parentIndex, 4);
    data += 4;
    for (int i = 0; i < node->bounds->mbr.size(); i++)
    {
        memcpy(data, &node->bounds->mbr[i].first, 4);
        data += 4;
    }
    for (int i = 0; i < node->bounds->mbr.size(); i++)
    {
        memcpy(data, &node->bounds->mbr[i].second, 4);
        data += 4;
    }
    int i = 0;
    while (i < node->childIndices.size())
    {
        memcpy(data, &node->childIndices[i], 4);
        data += 4;
        for (int j = 0; j < d; j++)
        {
            memcpy(data, &node->childBounds[i]->mbr[j].first, 4);
            data += 4;
        }
        for (int j = 0; j < d; j++)
        {
            memcpy(data, &node->childBounds[i]->mbr[j].second, 4);
            data += 4;
        }
        i++;
    }
}

Node *getNodeData(char *data, int node_id)
{
    while (*((int *)(data)) != node_id)
        data += NodeSize;

    return extractNodeData(data);
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
    n->childIndices.clear();
    n->childIndices.shrink_to_fit();
    for (int i = 0; i < n->childBounds.size(); i++)
    {
        deleteBounds(n->childBounds[i]);
    }
    n->childBounds.clear();
    n->childBounds.shrink_to_fit();
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
                    offsetData = 0;

                    fh.UnpinPage(startPageNumber);

                    startPageHandler = fh.NextPage(startPageNumber);
                    startPageNumber = startPageHandler.GetPageNum();

                    fh.MarkDirty(startPageNumber);
                    startData = startPageHandler.GetData();
                }

                Node *copyNode = extractNodeData(startData);
                memcpy((startData + sizeof(int)), &newNode->index, sizeof(int));

                copyNode->parentIndex = newNode->index;

                newNode->childIndices[nodeCount] = copyNode->index;

                startData = NodeSize + startData;
                offsetData = NodeSize + offsetData;
                for (int i = 0; i < d; i++)
                {
                    newNode->childBounds[nodeCount]->mbr[i].first = copyNode->bounds->mbr[i].first;
                    newNode->childBounds[nodeCount]->mbr[i].second = copyNode->bounds->mbr[i].second;

                    newBounds->mbr[i].first = min(newBounds->mbr[i].first, newNode->childBounds[nodeCount]->mbr[i].first);
                    newBounds->mbr[i].second = max(newBounds->mbr[i].second, newNode->childBounds[nodeCount]->mbr[i].second);
                }
                N--;
                nodeCount++;
                deleteNode(copyNode);
            }
            newNode->setBounds(newBounds);

            if (PAGE_CONTENT_SIZE - NodeSize < endPageOffset)
            {
                endPageOffset = 0;

                fh.UnpinPage(endPageNumber);

                endPageHandler = fh.NewPage();
                endPageNumber = endPageHandler.GetPageNum();

                fh.MarkDirty(endPageNumber);
                endData = endPageHandler.GetData();
            }
            numNodes += 1;
            copyNodeData(newNode, endData);
            deleteNode(newNode);
            endData += NodeSize;
            endPageOffset += NodeSize;
        }
    }
    catch (InvalidPageException)
    {
        cout << "[*] ERROR: Page is Invalid\n";
    }

    fh.UnpinPage(startPageNumber);
    fh.UnpinPage(endPageNumber);
    fh.FlushPages();

    if (numNodes != 1)
    {
        assignParents(end, end + numNodes, fm, fh);
    }
    else
    {
        rootIndex = focusIndex - 1;
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

                newNode->childIndices[nodeCount] = -1;
                for (int i = 0; i < d; i++)
                {
                    newNode->childBounds[nodeCount]->mbr[i].first = *(int *)(startData);
                    newNode->childBounds[nodeCount]->mbr[i].second = INT_MIN;
                    startData += sizeof(int);
                    pageOffset += sizeof(int);
                    newBounds->mbr[i].first = min(newBounds->mbr[i].first, newNode->childBounds[nodeCount]->mbr[i].first);
                    newBounds->mbr[i].second = max(newBounds->mbr[i].second, newNode->childBounds[nodeCount]->mbr[i].first);
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
                endPageOffset = 0;

                endFileHandler.UnpinPage(endPageNumber);

                endPageHandler = endFileHandler.NewPage();
                endPageNumber = endPageHandler.GetPageNum();

                endFileHandler.MarkDirty(endPageNumber);

                endData = endPageHandler.GetData();
            }

            copyNodeData(newNode, endData);
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
MBR *pointToBounds(vector<int> v)
{
    MBR *m = new MBR();
    for (int i = 0; i < m->mbr.size(); i++)
    {
        m->mbr[i].first = v[i];
        m->mbr[i].second = v[i];
    }
    return m;
}

MBR *newPointArea(MBR *a, vector<int> p)
{
    MBR *b = pointToBounds(p);
    MBR *new_mbr = new_area_mbr(a, b);
    deleteBounds(b);
    return new_mbr;
}
// hyper_area
long int boundsArea(MBR *m)
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
bool compareBounds(MBR *a, MBR *b)
{
    for (int i = 0; i < a->mbr.size(); i++)
    {
        if (!(a->mbr[i].first == b->mbr[i].first && a->mbr[i].second == b->mbr[i].second))
            return false;
    }
    return true;
}

void printBounds(MBR *m)
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
    PageHandler pageHandler = fh.PageAt(page_id);

    char *data = pageHandler.GetData();
    Node *n = getNodeData(data, index);

    if (n->isLeaf() == false)
    {

        for (int i = 0; i < maxCap; i++)
        {
            if (n->childIndices[i] == INT_MIN)
            {
                break;
            }

            MBR *m = pointToBounds(P);
            int is_belong = 1;
            for (int k = 0; k < m->mbr.size(); k++)
            {
                if ((m->mbr[k].first < n->childBounds[i]->mbr[k].first) || (m->mbr[k].first > n->childBounds[i]->mbr[k].second))
                {
                    is_belong = 0;
                    break;
                }
            }
            if (is_belong == 1)
            {
                if (search(P, fh, n->childIndices[i]))
                {
                    deleteBounds(m);
                    deleteNode(n);
                    fh.UnpinPage(pageHandler.GetPageNum());
                    return true;
                }
            }
            deleteBounds(m);
        }
    }
    else
    {
        for (int i = 0; i < maxCap; i++)
        {
            MBR *m = pointToBounds(P);
            int is_same = 1;
            for (int j = 0; j < P.size(); j++)
            {
                if ((P[j] != n->childBounds[i]->mbr[j].first))
                {
                    is_same = 0;
                    break;
                }
            }
            if (is_same == 1)
            {
                deleteBounds(m);
                deleteNode(n);
                fh.UnpinPage(pageHandler.GetPageNum());
                return true;
            }
            deleteBounds(m);
        }
        deleteNode(n);
        fh.UnpinPage(pageHandler.GetPageNum());
        return false;
    }
    deleteNode(n);
    fh.UnpinPage(pageHandler.GetPageNum());
    return false;
}

int main(int argc, char *argv[])
{
    FileHandler startFileHandler, endFileHandler;
    FileManager fileManager;

    ifstream infile(argv[1]);
    d = stoi(argv[3]);
    maxCap = stoi(argv[2]);

    NodeSize = (1 + 1 + 2 * d + maxCap + 2 * d * maxCap) * sizeof(int);

    string input;

    try
    {
        endFileHandler = fileManager.CreateFile("answer.txt");
    }
    catch (InvalidPageException)
    {
        fileManager.DestroyFile("answer.txt");
        endFileHandler = fileManager.CreateFile("answer.txt");
    }

    ofstream myfile;
    myfile.open(argv[4]);
    // NOTE : sizeof(pair<int,int>) = sizeof(int)*2

    while (getline(infile, input))
    {
        int position = input.find(" ");
        string command = input.substr(0, position);

        input.erase(0, position + 1);

        if (command == "QUERY")
        {
            vector<int> searchPoint;
            for (int dim = 0; dim < d; dim++)
            {
                position = input.find(" ");
                searchPoint.push_back(stoi(input.substr(0, position)));
                input.erase(0, 1 + position);
            }
            if (search(searchPoint, endFileHandler, rootIndex) == false)
            {
                myfile << "FALSE\n\n\n";
            }
            else
            {
                myfile << "TRUE\n\n\n";
            }

            endFileHandler.FlushPages();
            // myfile << endl;
        }

        if (command == "BULKLOAD")
        {
            position = input.find(" ");

            string inputfile = input.substr(0, position);

            startFileHandler = fileManager.OpenFile(inputfile.c_str());
            input.erase(0, 1 + position);

            position = input.find(" ");
            int N = stoi(input.substr(0, position));
            input.erase(0, 1 + position);

            bulkLoad(N, fileManager, startFileHandler, endFileHandler);

            myfile << "BULKLOAD\n\n\n";

            fileManager.CloseFile(startFileHandler);
        }

        if (command == "INSERT")
        {
            vector<int> searchPoint;
            for (int dim = 0; dim < d; dim++)
            {
                position = input.find(" ");
                searchPoint.push_back(stoi(input.substr(0, position)));
                input.erase(0, 1 + position);
            }

            if (rootIndex != -1)
            {
                // insertNode(endFileHandler, fileManager, searchPoint, rootIndex);
            }
            else
            {
                PageHandler pageHandler = endFileHandler.NewPage();

                char *data = pageHandler.GetData();
                Node *newNode = new Node();

                newNode->childIndices[0] = -1;
                newNode->index = focusIndex++;
                newNode->parentIndex = -1;

                for (int i = 0; i < d; i++)
                {
                    newNode->bounds->mbr[i].first = searchPoint[i];
                    newNode->bounds->mbr[i].second = searchPoint[i];
                    newNode->childBounds[0]->mbr[i].first = searchPoint[i];
                }
                rootIndex = 0;

                copyNodeData(newNode, data);
                endFileHandler.UnpinPage(pageHandler.GetPageNum());
                endFileHandler.FlushPage(pageHandler.GetPageNum());
            }

            myfile << "INSERT\n\n\n";

            endFileHandler.FlushPages();
        }
    }

    fileManager.CloseFile(endFileHandler);
    fileManager.DestroyFile("answer.txt");

    infile.close();
    myfile.close();

    return (0);
}