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

// Declare the class so SWIG registers a destructor, enabling own=true to actually free objects.
%nodefaultctor CollectionType;
class CollectionType {
public:
  ~CollectionType();
};

%typemap(in) CollectionType* (bool was_allocated = false) {
  // Assume collection, if it fails, interpret as Tcl list
  int res = SWIG_Tcl_ConvertPtr(interp, $input, (void**)&$1, $descriptor(CollectionType *), 0);
  if (!SWIG_IsOK(res)) {
    $1 = tclListSeqPtr<ElementType>($input, $descriptor(ElementType), interp);
    was_allocated = true;
  }
}

%typemap(freearg) CollectionType* {
  if (was_allocated$argnum) delete $1;
}

%typemap(in) const CollectionType* (bool was_allocated = false) {
  int res = SWIG_Tcl_ConvertPtr(interp, $input, (void**)&$1, $descriptor(CollectionType *), 0);
  if (!SWIG_IsOK(res)) {
    $1 = tclListSeqPtr<ElementType>($input, $descriptor(ElementType), interp);
    was_allocated = true;
  }
}

%typemap(freearg) const CollectionType* {
  if (was_allocated$argnum) delete $1;
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
    Tcl_Obj *obj;
    if ($1.size()) {
        auto *copy = new CollectionType($1);
        obj = SWIG_NewInstanceObj(copy, $descriptor(CollectionType *), true);
    } else {
        obj = Tcl_NewStringObj("", 0);
    }
    Tcl_SetObjResult(interp, obj);
  } else {
    seqTclList<CollectionType, ElementBaseType>($1, $descriptor(ElementType), interp);
  }
}

%typemap(out) CollectionType* {
  if (Sta::sta()->enableCollections()) {
    Tcl_Obj *obj;
    if ($1 && $1->size()) {
        obj = SWIG_NewInstanceObj($1, $descriptor(CollectionType *), true);
    } else {
        // backwards compatibility: return empty string, free the (empty) allocation
        delete $1;
        obj = Tcl_NewStringObj("", 0);
    }
    Tcl_SetObjResult(interp, obj);
  } else {
    if ($1) {
      seqPtrTclList<CollectionType, ElementBaseType>($1, $descriptor(ElementType), interp);
    } else {
      Tcl_SetObjResult(interp, Tcl_NewListObj(0, nullptr));
    }
    delete $1;
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
    IteratorType *collection_get_iterator(const CollectionType *v) {
        return new IteratorType(v);
    }
    void collection_sort_inplace(CollectionType *v, StringSeq *property_names, bool descending = false, bool natural = true) {
        auto network = Sta::sta()->network();
        auto &properties = Sta::sta()->properties();
        sta::sort(
            v,
            [&](ElementType A, ElementType B) {
                for (auto *property_name: *property_names) {
                    auto propertyA = properties.getProperty(A, property_name);
                    auto propertyB = properties.getProperty(B, property_name);
                    int diff = propertyA.compare(propertyB, network, natural);
                    if (diff != 0) {
                        return descending ? diff > 0 : diff < 0;
                    }
                }
                // all properties equal, thus A is not less than B
                return false;
            }
        );
    }

    CollectionType *collection_sorted(const CollectionType *v, StringSeq *property_names, bool descending = false, bool natural = true) {
        CollectionType *result;
        if (v != nullptr) {
            result = new CollectionType(*v);
        } else {
            result = new CollectionType();
        }
        collection_sort_inplace(result, property_names, descending, natural);
        return result;
    }

    void collection_append_inplace(CollectionType *v, const CollectionType *q, bool unique = false) {
        assert(v != nullptr); // collection_plus should be used if v isn't already a collection
        if (q == nullptr) {
            return;
        }
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

    CollectionType *collection_plus(const CollectionType *v, const CollectionType *q, bool unique = false) {
        CollectionType *result;
        if (v != nullptr) {
            result = new CollectionType(*v);
        } else {
            result = new CollectionType();
        }
        collection_append_inplace(result, q, unique);
        return result;
    }

    CollectionType *collection_slice(const CollectionType *v, const char *index1, const char *index2) {
        auto result = new CollectionType();
        size_t index1_resolved = resolve_index(index1, v->size());
        size_t index2_resolved = resolve_index(index2, v->size());
        if (index2_resolved < index1_resolved) {
            return result; // empty slice
        }
        result->reserve(index2_resolved - index1_resolved + 1);
        for (size_t i = index1_resolved; i <= index2_resolved && i < v->size(); i += 1) {
            result->push_back(v->at(i));
        }
        return result;
    }

    ElementType collection_element_at(const CollectionType *v, const char *index) {
        return v->at(resolve_index(index, v->size()));
    }

    size_t collection_count(const CollectionType *v) {
        return v->size();
    }

    CollectionType *collection_minus(const CollectionType *v, const CollectionType *q, bool intersect = false) {
        if (q == nullptr) {
            return intersect ? new CollectionType() : new CollectionType(*v);
        }
        std::unordered_set<ElementType> peer;
        for (ElementType e: *q) {
            peer.insert(e);
        }
        auto result = new CollectionType();
        for (ElementType e: *v) {
            if (intersect != (peer.count(e) == 0)) {
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
