#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Button.H>

#include <string>
#include <memory>
#include "docudb.hpp"


struct vm_s {
    std::unique_ptr<docudb::database> db;
    std::vector<std::unique_ptr<docudb::db_document_ref>> documents;
    Fl_Tree_Prefs tree;

    vm_s() : db(std::make_unique<docudb::database>("docui.db")), tree{} {
    }
} vm;

struct ui_s {
    Fl_Tree* tree{nullptr};
} ui;

struct collection_tag_s {} collection_tag;

bool is_collection(Fl_Tree_Item* const itm) {
    return itm->user_data() == &collection_tag;
}

// fw declarations
void add_document_cb(Fl_Widget*, void* tree_item_ptr);
void open_file_cb(Fl_Widget*, void* text_display);
void add_collection_cb(Fl_Widget*, void*);
void tree_cb(Fl_Widget*, void* text_display);
//

void add_add_document_button(Fl_Tree_Item* item) {
    auto button = new Fl_Button(0, 0, 100, 100, "Add Document");
    button->callback(add_document_cb, item);
    auto button_item = ui.tree->add(item, "Add Document");
    button_item->widget(button);
}

// Callback for adding a document
void add_document_cb(Fl_Widget*, void* tree_item_ptr) {
    Fl_Tree_Item* tree_item = static_cast<Fl_Tree_Item*>(tree_item_ptr);
    if (!tree_item) return;

    std::string collection_name = tree_item->label();
    auto collection = vm.db->collection(collection_name);

    auto doc = collection.doc();
    // Add the document to the tree view    
    auto item = ui.tree->insert_above(tree_item->find_clicked({}), doc.id().c_str());

    auto docref = std::make_unique<docudb::db_document_ref>(doc);
    auto docref_ptr = docref.get();
    vm.documents.push_back(std::move(docref));

    item->user_data(docref_ptr);
    ui.tree->redraw();
}

void clear_treeview() {
    vm.documents.clear();
    ui.tree->clear();
}

void load_database() {
    for(auto&& c: vm.db->collections()) {
        auto item = ui.tree->add(c.name().c_str());
        
        item->user_data(&collection_tag);
        for(auto&& d: c.docs()) {
            auto item = ui.tree->add((c.name() + "/" + d.id()).c_str());
            auto docref = std::make_unique<docudb::db_document_ref>(d);
            auto docref_ptr = docref.get();
            vm.documents.push_back(std::move(docref));
            item->user_data(docref_ptr);
        }

        add_add_document_button(item);
    }
    ui.tree->redraw();
}

// Callback for opening files
void open_file_cb(Fl_Widget*, void* text_display) {
    Fl_Text_Display* display = static_cast<Fl_Text_Display*>(text_display);
    const char* filename = fl_file_chooser("Open File", "*.db;*.sqlite;*.docudb", NULL);
    if (filename) {

        vm.db = std::make_unique<docudb::database>(filename);
        load_database();
    }
}

// Callback for adding a collection
void add_collection_cb(Fl_Widget*, void*) {
    // Create a window to ask for the collection name
    Fl_Window* input_window = new Fl_Window(300, 100, "Add Collection");
    Fl_Input* input = new Fl_Input(100, 20, 180, 30, "Name:");
    Fl_Button* button = new Fl_Button(100, 60, 80, 30, "Add");

    // Callback for the button
    button->callback([](Fl_Widget*, void* name_input) {
        Fl_Input* fl_input = static_cast<Fl_Input*>(name_input);
        std::string collection_name = fl_input->value();
        if (!collection_name.empty()) {
            Fl_Group::current(ui.tree);

            // Add the collection to the database
            vm.db->collection(collection_name);

            auto item = ui.tree->add(collection_name.c_str());
            add_add_document_button(item);
            ui.tree->redraw();
        }
        fl_input->window()->hide();
    }, input);

    input_window->end();
    input_window->set_modal();
    input_window->show();
}

// Callback for tree selection
void tree_cb(Fl_Widget* widget, void* text_display) {
    Fl_Tree* tree = static_cast<Fl_Tree*>(widget);
    Fl_Text_Display* display = static_cast<Fl_Text_Display*>(text_display);
    Fl_Tree_Item* item = tree->item_clicked();

    if (item->label() != "" &&  !is_collection(item) && item->user_data() != nullptr) {
        auto docref = reinterpret_cast<docudb::db_document_ref*>(item->user_data());
        Fl_Text_Buffer* buffer = new Fl_Text_Buffer();
        buffer->text(docref->doc().body().c_str());
        display->buffer(buffer);
    }
}

int main(int argc, char** argv) {
    Fl_Window* window = new Fl_Window(800, 600, "DocuDB UI");

    // Menu bar
    Fl_Menu_Bar* menu = new Fl_Menu_Bar(0, 0, 800, 25);
    menu->add("File/Open", FL_CTRL + 'o', open_file_cb);
    menu->add("Database/Add Collection", FL_CTRL + 'n', add_collection_cb);

    // Text display
    Fl_Text_Display* text_display = new Fl_Text_Display(200, 25, 600, 575);    

    // Sidebar with tree view
    ui.tree = new Fl_Tree(0, 25, 200, 575);
    load_database();
    ui.tree->callback(tree_cb, text_display);

    window->end();
    window->show(argc, argv);
    return Fl::run();
}
