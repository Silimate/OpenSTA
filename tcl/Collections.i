// IteratorType must be typedef'd
%inline {
    size_t resolve_index(const char *input, size_t collection_size) {
        if (strncmp(input, "end", 3) == 0) {
            size_t result = collection_size - 1;
            const char *rem = input + 3;
            if (*rem == '-') {
                rem += 1;
                result -= std::stoull(rem);
                return result;
            } else if (*rem != '\0') {
                throw std::invalid_argument("indices can only be a decimal integer, end, or end-decimal integer");
            }
            return result;
        }
        return std::stoull(input);
    }
}

%define COLLECTION_HELPERS(CollectionType, ElementType, IteratorType)

class IteratorType {
private:
  IteratorType();
  ~IteratorType();
};

%inline {
    IteratorType *get_iterator(const CollectionType *v) {
        return new IteratorType(v);
    }
    void sort_collection_by_properties(CollectionType *v, const char *property_names, bool descending = false) {
        auto network = Sta::sta()->network();
        auto properties = Sta::sta()->properties();
        std::vector<std::string> property_names_parsed;
        std::stringstream property_names_stream(property_names);
        std::string property_name;
        while (property_names_stream >> property_name) {
            property_names_parsed.push_back(property_name);
        }
        sta::sort(
            v,
            [&](ElementType A, ElementType B) {
                for (const auto &property_name: property_names_parsed) {
                    auto propertyA = properties.getProperty(A, property_name);
                    auto propertyB = properties.getProperty(B, property_name);
                    int diff = propertyA.compare(propertyB, network);
                    if (diff != 0) {
                        return descending ? diff > 0 : diff < 0;
                    }
                }
                // all properties equal, thus A is not less than B
                return descending;
            }
        );
    }

    CollectionType *collection_sorted_by_properties(const CollectionType *v, const char *property_names, bool descending = false) {
        auto result = new CollectionType(*v);
        sort_collection_by_properties(result, property_names, descending);
        return result;
    }

    void append_to_collection(CollectionType *v, const CollectionType *q) {
        v->reserve(v->size() + q->size());
        for (ElementType e: *q) {
            v->push_back(e);
        }
    }

    CollectionType *concat_collection(const CollectionType *v, const CollectionType *q) {
        auto result = new CollectionType(*v);
        append_to_collection(result, q);
        return result;
    }

    CollectionType *slice_collection(const CollectionType *v, const char *index1, const char *index2) {
        size_t index1_resolved = resolve_index(index1, v->size());
        size_t index2_resolved = resolve_index(index2, v->size());
        auto result = new CollectionType();
        if (index1_resolved == index2_resolved) {
            result->push_back(v->at(index1_resolved));
            return result;
        }
        result->reserve(index2_resolved - index1_resolved + 1);
        for (size_t i = 0; i < v->size(); i += 1) {
            if (i >= index1_resolved && i <= index2_resolved) {
                result->push_back(v->at(i));
            }
        }
        return result;
    }

    ElementType collection_at_index(const CollectionType *v, const char *index) {
        return v->at(resolve_index(index, v->size()));
    }

    size_t count_collection(const CollectionType *v) {
        return v->size();
    }

    bool confirm_collection(const CollectionType *v) {
        // testing for this method's existence should verify that the object is
        // a collection without maintaining a list of collections separately
        return true;
    }

    CollectionType *new_collection_removing(const CollectionType *v, const CollectionType *q) {
        std::unordered_set<ElementType> to_delete;
        for (ElementType e: *q) {
            to_delete.insert(e);
        }
        auto result = new CollectionType();
        for (ElementType e: *v) {
            if (to_delete.find(e) == to_delete.end()) {
                result->push_back(e);
            }
        }
        return result;
    }
}

%extend IteratorType {
    bool has_next() { return self->hasNext(); }
    ElementType next() { return self->next(); }
    void finish() { delete self; }
}

%typemap(out) IteratorType* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%enddef
