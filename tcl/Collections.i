// IteratorType must be typedef'd
%inline {
    size_t resolve_index(const char *input, size_t collection_size) {
        if (strncmp(input, "end", 3) == 0) {
            size_t result = collection_size - 1;
            const char *rem = input + 3;
            if (*rem == '-') {
                rem += 1;
                size_t offset = std::stoull(rem);
                if (offset <= result) {
                    return result - offset;
                } else {
                    return 0;
                }
            } else if (*rem != '\0') {
                throw std::invalid_argument("indices can only be a decimal integer, end, or end-decimal integer");
            }
            return result;
        }
        return std::stoull(input);
    }
}

%define COLLECTION_TYPEMAPS(CollectionType, ElementType, ElementBaseType)

%typemap(in) CollectionType* {
  // Assume collection, if it fails, interpret as Tcl list
  int res = SWIG_Tcl_ConvertPtr(interp, $input, (void**)&$1, $descriptor(CollectionType *), 0);
  if (!SWIG_IsOK(res)) {
    $1 = tclListSeqPtr<ElementType>($input, $descriptor(ElementType), interp);
  }
}

%typemap(typecheck, precedence=SWIG_TYPECHECK_POINTER) CollectionType* {
  void *obj;
  int res = SWIG_Tcl_ConvertPtr(interp, $input, (void**)&obj, $descriptor(CollectionType *), 0);
  if (SWIG_IsOK(res)) {
    $1 = 1;
  } else {
    $1 = tclCheckListSeq<ElementType>($input, $descriptor(ElementType), interp);
  }
}

%typemap(out) CollectionType {
  if (Sta::sta()->enableCollections()) {
    auto *copy = new CollectionType($1);
    Tcl_Obj *obj = SWIG_NewInstanceObj(copy, $descriptor(CollectionType *), true);
    Tcl_SetObjResult(interp, obj);
  } else {
    seqTclList<CollectionType, ElementBaseType>($1, $descriptor(ElementType), interp);
  }
}

%typemap(out) CollectionType* {
  if (Sta::sta()->enableCollections()) {
    Tcl_Obj *obj = SWIG_NewInstanceObj($1, $descriptor(CollectionType *), true);
    Tcl_SetObjResult(interp, obj);
  } else {
    seqPtrTclList<CollectionType, ElementBaseType>($1, $descriptor(ElementType), interp);
  }
}

%enddef

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
    void sort_collection_by_properties(CollectionType *v, StringSeq *property_names, bool descending = false) {
        auto network = Sta::sta()->network();
        auto properties = Sta::sta()->properties();
        sta::sort(
            v,
            [&](ElementType A, ElementType B) {
                for (auto *property_name: *property_names) {
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

    CollectionType *collection_sorted_by_properties(const CollectionType *v, StringSeq *property_names, bool descending = false) {
        auto result = new CollectionType(*v);
        sort_collection_by_properties(result, property_names, descending);
        return result;
    }

    void append_to_collection_inplace(CollectionType *v, const CollectionType *q, bool unique = false) {
        v->reserve(v->size() + q->size());
        if (unique) {
            std::unordered_set<ElementType> unique_elements;
            for (ElementType e: *v) {
                unique_elements.insert(e);
            }
            for (ElementType e: *q) {
                if (unique_elements.count(e)) {
                    continue;
                }
                v->push_back(e);
                unique_elements.insert(e);
            }
            v->shrink_to_fit();
        } else {
            for (ElementType e: *q) {
                v->push_back(e);
            }
        }
    }

    CollectionType *concat_collection(const CollectionType *v, const CollectionType *q, bool unique = false) {
        auto result = new CollectionType(*v);
        append_to_collection_inplace(result, q, unique);
        return result;
    }

    CollectionType *slice_collection(const CollectionType *v, const char *index1, const char *index2) {
        size_t index1_resolved = resolve_index(index1, v->size());
        size_t index2_resolved = resolve_index(index2, v->size());
        auto result = new CollectionType();
        if (index2_resolved < index1_resolved) {
            return result; // empty slice
        }
        result->reserve(index2_resolved - index1_resolved + 1);
        for (size_t i = index1_resolved; i <= index2_resolved && i < v->size(); i += 1) {
            result->push_back(v->at(i));
        }
        return result;
    }

    ElementType collection_at_index(const CollectionType *v, const char *index) {
        return v->at(resolve_index(index, v->size()));
    }

    size_t count_collection(const CollectionType *v) {
        return v->size();
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
