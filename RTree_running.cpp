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
        this->child_index.resize(maxCap, INT_MIN);
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




// store node in char* data
Node *extract_node(char *data)
{
    Node *n = new Node();
    // assign all values to node variables
    n->index = *(int *)(data);
    data += sizeof(int);
    n->parent_index = *(int *)(data);
    data += 4;
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

    for (int i = 0; i < n->child_index.size(); i++)
    {
        n->child_index[i] = *(int *)(data);
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
    cout << "Copy start" << endl;
    memcpy(data, &n->index, sizeof(int));
    data += sizeof(int);
    memcpy(data, &n->parent_index, sizeof(int));
    data += sizeof(int);
    cout << "Copy mid1" << endl;
    for (int i = 0; i < n->bounds->mbr.size(); i++)
    {
        memcpy(data, &n->bounds->mbr[i].first, sizeof(int));
        data += sizeof(int);
    }
    cout << "Copy mid2" << endl;
    for (int i = 0; i < n->bounds->mbr.size(); i++)
    {
        memcpy(data, &n->bounds->mbr[i].second, sizeof(int));
        data += sizeof(int);
    }
    cout << "Copy mid3" << endl;
    for (int i = 0; i < n->child_index.size(); i++)
    {
        cout << "Copy mid4" << endl;
        memcpy(data, &n->child_index[i], sizeof(int));
        data += sizeof(int);
        cout << "dimensionality: " << d << endl;
        for (int j = 0; j < d; j++)
        {
            cout << "Copy mid5" << endl;
            memcpy(data, &n->child_bounds[i]->mbr[j].first, sizeof(int));
            data += sizeof(int);
        }
        cout << "Copy mid6" << endl;
        for (int j = 0; j < d; j++)
        {
            memcpy(data, &n->child_bounds[i]->mbr[j].second, sizeof(int));
            data += sizeof(int);
        }
    }
    cout << "Copy end" << endl;
}

Node *get_node(char *data, int node_id)
{
    while (*((int *)(data)) != node_id)
        data += NodeSize;

    return extract_node(data);
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

void assignParents(int start, int end, FileManager &fm, FileHandler &fh)
{
    // N = Number of nodes bytes to be inserted into the tree
    int N = end - start;
    int numNodes = 0;
    int startPageNumber, endPageNumber;
    cout << "assign 1" << endl;
    try
    {
        // startPage tells the page to look at for data
        int startPage = start / (PAGE_CONTENT_SIZE / NodeSize);
        cout << "assign 2" << endl;

        // PageHandler startPageHandler = fh.PageAt(startPage);
        // PageHandler endPageHandler = fh.LastPage();

        // // pointers to current data read location and last data read location
        // char *startData, *endData;

        // startData = startPageHandler.GetData();
        // endData = endPageHandler.GetData();
        // endData += endPageOffset;

        // startPageNumber = startPageHandler.GetPageNum();
        // endPageNumber = endPageHandler.GetPageNum();

        // fh.MarkDirty(startPageNumber);
        // fh.MarkDirty(endPageNumber);


        char *startData, *endData;
        PageHandler startPageHandler = fh.PageAt(startPage);
        startData = startPageHandler.GetData();
        startPageNumber = startPageHandler.GetPageNum();
        fh.MarkDirty(startPageNumber);

        PageHandler endPageHandler = fh.LastPage();
        endPageNumber = endPageHandler.GetPageNum();
        fh.MarkDirty(endPageNumber);

        endData = endPageHandler.GetData();
        endData = endData + endPageOffset;

        int offsetData = 0;
        while (*((int *)(startData)) != start)
        {
            cout << "assign 3" << endl;
            startData += NodeSize;
            offsetData += NodeSize;
        }

        while (N > 0)
        {
            cout << "assign 4" << endl;
            Node *newNode = new Node();
            newNode->index = focusIndex;
            focusIndex += 1;

            int nodeCount = 0;

            MBR *newBounds = new MBR(d);
            for (int i = 0; i < d; i++)
            {
                cout << "assign 5" << endl;
                newBounds->mbr[i].first = INT_MAX;
                newBounds->mbr[i].second = INT_MIN;
            }

            while (N > 0 && maxCap > nodeCount)
            {
                cout << "assign 6" << endl;
                if (PAGE_CONTENT_SIZE - NodeSize < offsetData)
                {
                    cout << "assign 6.1" << endl;
                    fh.UnpinPage(startPageNumber);

                    startPageHandler = fh.NextPage(startPageNumber);
                    startPageNumber = startPageHandler.GetPageNum();
                    fh.MarkDirty(startPageNumber);

                    startData = startPageHandler.GetData();
                    offsetData = 0;
                }
                cout << "assign 6.2" << endl;

                Node *copyNode = extract_node(startData);
                cout << "assign 6.3" << endl;
                memcpy((startData + sizeof(int)), &newNode->index, sizeof(int));

                copyNode->parent_index = newNode->index;

                newNode->child_index[nodeCount] = copyNode->index;

                startData = NodeSize + startData;
                offsetData = NodeSize + offsetData;
                for (int i = 0; i < d; i++)
                {
                    cout << "assign 6.4" << endl;
                    newNode->child_bounds[nodeCount]->mbr[i].first = copyNode->bounds->mbr[i].first;
                    newNode->child_bounds[nodeCount]->mbr[i].second = copyNode->bounds->mbr[i].second;

                    newBounds->mbr[i].first = min(newBounds->mbr[i].first, newNode->child_bounds[nodeCount]->mbr[i].first);
                    newBounds->mbr[i].second = max(newBounds->mbr[i].second, newNode->child_bounds[nodeCount]->mbr[i].second);
                }
                N--;
                nodeCount++;
                delete_node(copyNode);
            }
            cout << "assign 7" << endl;
            // newNode->setLeftBounds(newBounds);
            // newNode->setRightBounds(newBounds);
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

            cout << "assign 8" << endl;
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
    cout << "assign 9" << endl;
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

            for(int i=0;i<d;i++){
                newBounds->mbr[i].first = INT_MAX;
                newBounds->mbr[i].second = INT_MIN;
            }



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
            for(int i=0;i<d;i++){
                if(newBounds->mbr[i].first != newNode->bounds->mbr[i].first || newBounds->mbr[i].second != newNode->bounds->mbr[i].second)
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
            delete_node(newNode);
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
    delete_mbr(b);
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

void print_mbr(MBR *m){
    cerr << "Printing MBR \n";
    for(int i=0;i<m->mbr.size();i++){
        cerr << m->mbr[i].first << " " << m->mbr[i].second << "\n";
    }
    return;
}

bool search(vector<int> &P, FileHandler &fh, int index)
{
    int page_id = index / (PAGE_CONTENT_SIZE / NodeSize);
    PageHandler ph = fh.PageAt(page_id);
    char *data = ph.GetData();
    Node *n = get_node(data, index);

    if (n->isLeaf())
    {
        for (int i = 0; i < maxCap; i++)
        {
            MBR *m = point_mbr(P);
            int is_same = 1;
            // for(int j=0;j<m->mbr.size(); j++){
            for(int j=0;j<P.size(); j++){
                // if((m->mbr[j].first != n->child_bounds[i]->mbr[j].first))
                if((P[j] != n->child_bounds[i]->mbr[j].first))
                {
                    // cerr << "Some print\n";
                    // cerr << P[j] << " " << n->child_bounds[i]->mbr[j].first << endl; 
                    is_same = 0;
                    break; 
                }
            }
            // cerr << "IS same " << is_same << endl;
            // if (is_equal(m, n->child_bounds[i]))
            if(is_same == 1)
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
            if (n->child_index[i] == INT_MIN){
                // cerr << "Goes to min value\n";
                break;
            }

            MBR *m = point_mbr(P);
            // cerr << "Print mbr m\n";
            // print_mbr(m);
            // cerr << "Print mbr n bounds\n";
            // print_mbr(n->child_bounds[i]);
            int is_belong = 1;
            // cerr << "Random text\n";
            for(int k=0;k<m->mbr.size();k++){
                if((m->mbr[k].first < n->child_bounds[i]->mbr[k].first) || (m->mbr[k].first > n->child_bounds[i]->mbr[k].second))
                {
                    // cerr << m->mbr[k].first << "   " << n->child_bounds[i]->mbr[k].first << "  " << n->child_bounds[i]->mbr[k].second << "\n";
                    is_belong = 0;
                    break;
                }
            }
            // if (belongs(m, n->child_bounds[i]))
            if(is_belong == 1)
            {
                // cerr << "Search 3.1" << endl;
                if (search(P, fh, n->child_index[i]))
                {
                    delete_mbr(m);
                    delete_node(n);
                    fh.UnpinPage(ph.GetPageNum());
                    return true;
                }
            }
            // cerr << "Search 3.2\n";
            delete_mbr(m);
        }
    }
    delete_node(n);
    fh.UnpinPage(ph.GetPageNum());
    return false;
}

int main(int argc, char *argv[])
{
    cout << "HERE\n";
    FileManager fm;
    FileHandler startFileHandler, endFileHandler;
    string input;

    ifstream infile(argv[1]);
    d = stoi(argv[2]);
    maxCap = stoi(argv[3]);

    try
    {
        endFileHandler = fm.CreateFile("answer.txt");
    }
    catch (InvalidPageException)
    {
        fm.DestroyFile("answer.txt");
        endFileHandler = fm.CreateFile("answer.txt");
    }

    ofstream outfile(argv[4]);

    // NOTE : sizeof(pair<int,int>) = sizeof(int)*2
    NodeSize = (1 + 1 + 2 * d + maxCap + 2 * d * maxCap) * sizeof(int);

    while (getline(infile, input))
    {
        int position = input.find(" ");
        string command = input.substr(0, position);
        input.erase(0, position + 1);

        if (command == "BULKLOAD")
        {
            position = input.find(" ");
            string inputfile = input.substr(0, position);
            cout << inputfile << endl;
            startFileHandler = fm.OpenFile(inputfile.c_str());
            input.erase(0, 1 + position);

            position = input.find(" ");
            int N = stoi(input.substr(0, position));
            input.erase(0, 1 + position);

            bulkLoad(N, fm, startFileHandler, endFileHandler);
            cout << "Bulkload3" << endl;

            outfile << "BULKLOAD\n\n";

            fm.CloseFile(startFileHandler);
            cout << "Bulkload4" << endl;
        }
        else if (command == "QUERY")
        {
            cout << "Query" << endl;
            vector<int> searchPoint;
            for (int i = 0; i < d; i++)
            {
                position = input.find(" ");
                searchPoint.push_back(stoi(input.substr(0, position)));
                input.erase(0, 1 + position);
            }
            if (search(searchPoint, endFileHandler, rootIndex))
            {
                outfile << "TRUE" << endl;
                cerr << "TRUE" << endl;
            }
            else
            {
                cerr << "False" << endl;
                outfile << "FALSE" << endl;
            }

            endFileHandler.FlushPages();
            outfile << endl;
        }
    }

    fm.CloseFile(endFileHandler);
    fm.DestroyFile("answer.txt");

    infile.close();
    outfile.close();

    return (0);
}
